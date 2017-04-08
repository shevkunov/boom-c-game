TEMPLATE = app
CONFIG += console
CONFIG -= app_bundle
CONFIG -= qt
QMAKE_CFLAGS += -ansi -pedantic -Wall -Wextra
QMAKE_LFLAGS += -pthread

SOURCES += \
    boomclt.c

HEADERS += \
    boomlib.h \
    boommap.h \
    boomque.h

