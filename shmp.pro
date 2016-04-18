QT       -= core gui

TARGET = shmp
TEMPLATE = lib

DEFINES += SHMP_LIBRARY

SOURCES += shmp.cpp

HEADERS += shmp.h \
           shmp_decl.h \
           shmp_containers.h

INCLUDEPATH += C:/boost/boost_1_60_0

CONFIG(release, debug|release): DESTDIR = ./bin
CONFIG(debug, debug|release): DESTDIR = ./bin.debug
