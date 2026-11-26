QT += core gui qml quick quickcontrols2

CONFIG += c++17 release
TARGET = stopwatch
TEMPLATE = app

HEADERS += \
    src/stopwatch.h \
    src/theme.h \
    src/translator.h

SOURCES += \
    src/main.cpp \
    src/stopwatch.cpp \
    src/theme.cpp \
    src/translator.cpp

RESOURCES += src/resources.qrc

# The .exe's own icon in Explorer and the taskbar.
win32: RC_ICONS = pkgbuild/stopwatch.ico
