QT = core network widgets

CONFIG += static

HEADERS       = src/admin_check_page.h \
                src/deviceguy.h \
                src/diskwritethread.h \
                src/downloader.h \
                src/gondar.h \
                src/gondarwizard.h \
                src/image_select_page.h \
                src/neverware_unzipper.h \
                src/shared.h \
                src/unzipthread.h
SOURCES       = minizip/minishared.c \
                src/admin_check_page.cc \
                src/deviceguy.cc \
                src/diskwritethread.cc \
                src/downloader.cc \
                src/gondarwizard.cc \
                src/image_select_page.cc \
                src/main.cc \
                src/neverware_unzipper.c \
                src/unzipthread.cc
RESOURCES     = gondarwizard.qrc

INCLUDEPATH += minizip

win32 {
  RC_FILE       = gondar.rc
  INCLUDEPATH += ms-sys/inc
  SOURCES += src/gondar.c
} else {
  SOURCES += src/stubs.cc  
}

*gcc* {
  QMAKE_CFLAGS += -Wextra -Wmissing-declarations
  # Expose fopen64 in stdio.h declarations
  QMAKE_CFLAGS += -D_LARGEFILE64_SOURCE
}

*g++* {
  QMAKE_CXXFLAGS += -Wextra -Wmissing-declarations -std=c++11 -Wsuggest-override

  # Mark Qt headers in the MXE build as system headers. Without this
  # the above compiler warning options apply to Qt headers, which can
  # spew a lot of warnings.
  win32 {
    QMAKE_CXXFLAGS += -isystem ../mxe/usr/i686-w64-mingw32.static/qt5/include/QtCore
    QMAKE_CXXFLAGS += -isystem ../mxe/usr/i686-w64-mingw32.static/qt5/include/QtNetwork
    QMAKE_CXXFLAGS += -isystem ../mxe/usr/i686-w64-mingw32.static/qt5/include/QtWidgets
  }
}

# install
target.path = .
INSTALLS += target

TEMPLATE = app
TARGET = gondar

win32 {
  LIBS += -Lbuild/minizip-mxe
  LIBS += -lsetupapi -lole32 -lgdi32 -lwininet -lshlwapi -lcrypt32 -lwintrust -lcomdlg32 -luuid

  # Needed for static linking
  LIBS += -lqwindows

  equals(QT_MAJOR_VERSION, 5):lessThan(QT_MINOR_VERSION, 7) {
    # Needed for static linking in Qt5.6, but breaks the build in Qt5.8
    LIBS += -lQt5PlatformSupport
  }
} else {
  LIBS += -Lminizip
}

LIBS += -lminizip -lz -laes
