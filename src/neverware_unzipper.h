#ifndef NEVERWARE_UNZIPPER_H
#define NEVERWARE_UNZIPPER_H

#ifdef __cplusplus
extern "C" {
#endif

char* neverware_unzip(const char* url);
void get_filename_inside_zip(char* zipfile, char* filename);

#ifdef __cplusplus
}
#endif

#endif /* NEVERWARE_UNZIPPER_H */
