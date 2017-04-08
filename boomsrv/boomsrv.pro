TEMPLATE = app
CONFIG += console
CONFIG -= app_bundle
CONFIG -= qt
QMAKE_CFLAGS += -ansi -pedantic -Wall -Wextra
QMAKE_LFLAGS += -pthread

SOURCES += \
    boomsrv.c

HEADERS += \
    ../boomclt/boomlib.h \
    ../boomclt/boomque.h \
    ../boomclt/boomvec.h \
    ../boomclt/boommap.h

