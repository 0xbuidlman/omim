# fribidi library for unicode bidirectional algorithm

TEMPLATE = lib
CONFIG += staticlib

INCLUDEPATH += ./lib

ROOT_DIR = ../..
DEPENDENCIES =

include($$ROOT_DIR/common.pri)

HEADERS += \
    lib/run.h \
    lib/mem.h \
    lib/joining-types.h \
    lib/fribidi-unicode-version.h \
    lib/fribidi-unicode.h \
    lib/fribidi-types.h \
    lib/fribidi-shape.h \
    lib/fribidi-mirroring.h \
    lib/fribidi-joining-types-list.h \
    lib/fribidi-joining-types.h \
    lib/fribidi-joining.h \
    lib/fribidi-flags.h \
    lib/fribidi-enddecls.h \
    lib/fribidi-deprecated.h \
    lib/fribidi-config.h \
    lib/fribidi-common.h \
    lib/fribidi-bidi-types-list.h \
    lib/fribidi-bidi-types.h \
    lib/fribidi-bidi.h \
    lib/fribidi-begindecls.h \
    lib/fribidi-arabic.h \
    lib/fribidi.h \
    lib/debug.h \
    lib/common.h \
    lib/bidi-types.h

SOURCES += \
    lib/fribidi-shape.c \
    lib/fribidi-run.c \
    lib/fribidi-mirroring.c \
    lib/fribidi-mem.c \
    lib/fribidi-joining-types.c \
    lib/fribidi-joining.c \
    lib/fribidi-deprecated.c \
    lib/fribidi-bidi-types.c \
    lib/fribidi-bidi.c \
    lib/fribidi-arabic.c \
    lib/fribidi.c
