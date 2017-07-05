
// this code modified from minizip/miniunz.c

#include "neverware_unzipper.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <direct.h>
#include <io.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#include <utime.h>
#endif

#include "unzip.h"

#ifdef _WIN32
#define USEWIN32IOAPI
#include "iowin32.h"
#endif

#include <QDir>

#include "log.h"
#include "minishared.h"

// TODO(nicholasbishop): this file has been minimally converted from C
// to C++, so there are a lot of cleanups we can do (especially in the
// string operations)

static QString get_filename_inside_zip(unzFile uf) {
  int err = unzGoToFirstFile(uf);
  if (err != UNZ_OK) {
    LOG_ERROR << "error " << err << " with zipfile in unzGoToFirstFile";
    return QString();
  }

  constexpr int FILENAME_BUFFER_SIZE = 256;
  char filename[FILENAME_BUFFER_SIZE] = {};
  unz_file_info64 file_info = {};
  err = unzGetCurrentFileInfo64(uf, &file_info, filename, FILENAME_BUFFER_SIZE,
                                NULL, 0, NULL, 0);
  if (err != UNZ_OK) {
    LOG_ERROR << "unzGetCurrentFileInfo64 failed: " << err;
    return QString();
  }
  return filename;
}

static int miniunz_extract_currentfile(unzFile uf,
                                       int opt_extract_without_path,
                                       int* popt_overwrite,
                                       const char* password) {
  unz_file_info64 file_info = {};
  FILE* fout = NULL;
  void* buf = NULL;
  uint16_t size_buf = 8192;
  int err = UNZ_OK;
  int errclose = UNZ_OK;
  int skip = 0;
  char filename_inzip[256] = {0};
  char* filename_withoutpath = NULL;
  const char* write_filename = NULL;
  char* p = NULL;

  err = unzGetCurrentFileInfo64(uf, &file_info, filename_inzip,
                                sizeof(filename_inzip), NULL, 0, NULL, 0);
  if (err != UNZ_OK) {
    LOG_ERROR << "error " << err << " with zipfile in unzGetCurrentFileInfo";
    return err;
  }
  p = filename_withoutpath = filename_inzip;
  while (*p != 0) {
    if ((*p == '/') || (*p == '\\'))
      filename_withoutpath = p + 1;
    p++;
  }

  /* If zip entry is a directory then create it on disk */
  if (*filename_withoutpath == 0) {
    if (opt_extract_without_path == 0) {
      LOG_INFO << "creating directory: " << filename_inzip;
      MKDIR(filename_inzip);
    }
    return err;
  }

  buf = (void*)malloc(size_buf);
  if (buf == NULL) {
    LOG_ERROR << "Error allocating memory";
    return UNZ_INTERNALERROR;
  }

  err = unzOpenCurrentFilePassword(uf, password);
  if (err != UNZ_OK)
    LOG_ERROR << "error " << err
              << " with zipfile in unzOpenCurrentFilePassword";

  if (opt_extract_without_path)
    write_filename = filename_withoutpath;
  else
    write_filename = filename_inzip;

  /* Determine if the file should be overwritten or not and ask the user if
   * needed */
  if ((err == UNZ_OK) && (*popt_overwrite == 0) &&
      (check_file_exists(write_filename))) {
    char rep = 0;
    do {
      char answer[128];
      printf("The file %s exists. Overwrite ? [y]es, [n]o, [A]ll: ",
             write_filename);
      if (scanf("%1s", answer) != 1)
        exit(EXIT_FAILURE);
      rep = answer[0];
      if ((rep >= 'a') && (rep <= 'z'))
        rep -= 0x20;
    } while ((rep != 'Y') && (rep != 'N') && (rep != 'A'));

    if (rep == 'N')
      skip = 1;
    if (rep == 'A')
      *popt_overwrite = 1;
  }

  /* Create the file on disk so we can unzip to it */
  if ((skip == 0) && (err == UNZ_OK)) {
    fout = fopen64(write_filename, "wb");
    /* Some zips don't contain directory alone before file */
    if ((fout == NULL) && (opt_extract_without_path == 0) &&
        (filename_withoutpath != (char*)filename_inzip)) {
      char c = *(filename_withoutpath - 1);
      *(filename_withoutpath - 1) = 0;
      makedir(write_filename);
      *(filename_withoutpath - 1) = c;
      fout = fopen64(write_filename, "wb");
    }
    if (fout == NULL)
      LOG_ERROR << "error opening " << write_filename;
  }

  /* Read from the zip, unzip to buffer, and write to disk */
  if (fout != NULL) {
    LOG_INFO << " extracting: " << write_filename;

    do {
      err = unzReadCurrentFile(uf, buf, size_buf);
      if (err < 0) {
        LOG_ERROR << "error " << err << " with zipfile in unzReadCurrentFile";
        break;
      }
      if (err == 0)
        break;
      if (fwrite(buf, err, 1, fout) != 1) {
        LOG_ERROR << "error " << errno << " in writing extracted file";
        err = UNZ_ERRNO;
        break;
      }
    } while (err > 0);

    if (fout)
      fclose(fout);

    /* Set the time of the file that has been unzipped */
    if (err == 0)
      change_file_date(write_filename, file_info.dos_date);
  }

  errclose = unzCloseCurrentFile(uf);
  if (errclose != UNZ_OK)
    LOG_ERROR << "error " << errclose << " with zipfile in unzCloseCurrentFile";

  free(buf);
  return err;
}

static int miniunz_extract_all(unzFile uf,
                               int opt_extract_without_path,
                               int opt_overwrite,
                               const char* password) {
  int err = unzGoToFirstFile(uf);
  if (err != UNZ_OK) {
    LOG_ERROR << "error " << err << " with zipfile in unzGoToFirstFile";
    return 1;
  }

  do {
    err = miniunz_extract_currentfile(uf, opt_extract_without_path,
                                      &opt_overwrite, password);
    if (err != UNZ_OK)
      break;
    err = unzGoToNextFile(uf);
  } while (err == UNZ_OK);

  if (err != UNZ_END_OF_LIST_OF_FILE) {
    LOG_ERROR << "error " << err << " with zipfile in unzGoToNextFile";
    return 1;
  }
  return 0;
}

static void setCwd(const QDir& dir) {
  const QString path = dir.absolutePath();
  if (QDir::setCurrent(path)) {
    LOG_INFO << "set current working directory to " << path;
  } else {
    LOG_ERROR << "failed to set current working directory to " << path;
  }
}

QFileInfo neverware_unzip(const QFileInfo& input_file) {
  const std::string input_path = input_file.absoluteFilePath().toStdString();
  const char* zipfilename = input_path.c_str();
  unzFile uf = NULL;
#ifdef USEWIN32IOAPI
  zlib_filefunc64_def ffunc;
  fill_win32_filefunc64A(&ffunc);
  uf = unzOpen2_64(zipfilename, &ffunc);
#else
  uf = unzOpen64(zipfilename);
#endif

  if (uf == NULL) {
    LOG_ERROR << "Cannot open " << zipfilename;
    return QFileInfo();
  }

  LOG_INFO << zipfilename << " opened";

  const QString filename = get_filename_inside_zip(uf);
  const QFileInfo binpath = input_file.absoluteDir().absoluteFilePath(filename);

  // TODO(nicholasbishop): modify the extraction to not use the
  // current working directory
  const QDir origCwd = QDir::current();
  setCwd(input_file.absolutePath());
  int ret = miniunz_extract_all(uf,
                                /*extract_without_path*/ 1,
                                /*overwrite*/ 1,
                                /*password*/ NULL);
  setCwd(origCwd);

  unzClose(uf);
  if (ret != 0) {
    return QFileInfo();
  }

  return binpath;
}
