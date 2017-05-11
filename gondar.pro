QT = core network widgets

CONFIG += console static

HEADERS       = gondarwizard.h \
                downloader.h \
                deviceguy.h \
                shared.h \
                gondar.h
SOURCES       = gondarwizard.cc \
                downloader.cc \
                main.cc \
                deviceguy.cc
RESOURCES     = gondarwizard.qrc

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

win32 {
  LIBS += -lsetupapi -lole32 -lgdi32 -lwininet -lshlwapi -lcrypt32 -lwintrust -lcomdlg32 -luuid

  # Needed for static linking
  LIBS += -lqwindows
}
