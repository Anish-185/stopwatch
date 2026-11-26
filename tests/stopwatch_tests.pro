QT += core gui quick quickcontrols2 testlib
CONFIG += c++17 testcase
TARGET = stopwatch_tests
TEMPLATE = app

INCLUDEPATH += ../src

HEADERS += ../src/stopwatch.h ../src/theme.h ../src/translator.h
SOURCES += stopwatch_tests.cpp ../src/stopwatch.cpp ../src/theme.cpp ../src/translator.cpp
