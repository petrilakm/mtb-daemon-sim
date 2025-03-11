TARGET = mtb-daemon-sim
TEMPLATE = app
CONFIG += console

CONFIG -= app_bundle
CONFIG -= qtquickcompiler

SOURCES += \
  src/bmp_tlacitka.cpp \
	src/main.cpp \
	src/server.cpp \
	src/logging.cpp \
	src/qjsonsafe.cpp \
	src/modules/module.cpp \
	src/modules/unis.cpp \
  src/simwin.cpp

HEADERS += \
  src/bmp_tlacitka.h \
	src/main.h \
	src/mtbusb/mtbusb.h \
	src/server.h \
	src/logging.h \
	src/qjsonsafe.h \
	src/modules/module.h \
	src/modules/unis.h \
	src/errors.h \
  src/simwin.h \
	src/utils.h \
	lib/termcolor.h

INCLUDEPATH += \
	src \
	lib \
	src/mtbusb \
	src/modules

CONFIG += c++17
QMAKE_CXXFLAGS += -Wall -Wextra -pedantic -std=c++17

win32 {
	QMAKE_LFLAGS += -Wl,--kill-at
	QMAKE_CXXFLAGS += -enable-stdcall-fixup
	LIBS += -lsetupapi
}
win64 {
	QMAKE_LFLAGS += -Wl,--kill-at
	QMAKE_CXXFLAGS += -enable-stdcall-fixup
	LIBS += -lsetupapi
}

QT += gui
QT += core serialport network widgets

VERSION_MAJOR = 1
VERSION_MINOR = 0

DEFINES += "VERSION_MAJOR=$$VERSION_MAJOR" "VERSION_MINOR=$$VERSION_MINOR"

#Target version
VERSION = $${VERSION_MAJOR}.$${VERSION_MINOR}
DEFINES += "VERSION=\\\"$${VERSION}\\\""

RESOURCES += \
  src/resources.qrc
