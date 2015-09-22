TARGET = qt_tstfrm
TEMPLATE = lib
CONFIG += staticlib

ROOT_DIR = ..
DEPENDENCIES = map graphics geometry coding base

include($$ROOT_DIR/common.pri)

QT *= core gui opengl

HEADERS += \
  tstwidgets.hpp \
  macros.hpp \
    gl_test_widget.hpp \
    gui_test_widget.hpp

SOURCES += \
  tstwidgets.cpp \


