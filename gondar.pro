QT = core network widgets

CONFIG += console static

HEADERS       = gondarwizard.h \
                downloader.h \
                deviceguy.h \
                shared.h \
                gondar.h \
                neverware_unzipper.h
SOURCES       = gondarwizard.cc \
                downloader.cc \
                main.cc \
                deviceguy.cc \
                neverware_unzipper.c \
                minizip/minishared.c

INCLUDEPATH += minizip

win32 {
  INCLUDEPATH += ms-sys/inc
  SOURCES += gondar.c
} else {
  SOURCES += stubs.cc  
}

*gcc* {
  QMAKE_CFLAGS += -Wextra -Wmissing-declarations
}

*g++* {
  QMAKE_CXXFLAGS += -Wextra -Wmissing-declarations
}

# install
target.path = .
INSTALLS += target

TEMPLATE = app
TARGET = gondar

#TODO(kendall): win32 this and else minizip/native
LIBS += -Lminizip/build -lminizip -lz -laes

win32 {
  LIBS += -lsetupapi -lole32 -lgdi32 -lwininet -lshlwapi -lcrypt32 -lwintrust -lcomdlg32 -luuid

  # Needed for static linking
  LIBS += -lqwindows

  equals(QT_MAJOR_VERSION, 5):lessThan(QT_MINOR_VERSION, 7) {
    # Needed for static linking in Qt5.6, but breaks the build in Qt5.8
    LIBS += -lQt5PlatformSupport
  }
}
