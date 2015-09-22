# Storage library.

TARGET = storage
TEMPLATE = lib
CONFIG += staticlib

ROOT_DIR = ..

DEPENDENCIES = indexer geometry coding base jansson

include($$ROOT_DIR/common.pri)

INCLUDEPATH += $$ROOT_DIR/3party/jansson/src

HEADERS += \
  country.hpp \
  simple_tree.hpp \
  storage.hpp \
  country_polygon.hpp \
  country_info.hpp \
  country_decl.hpp \
  index.hpp \

SOURCES += \
  country.cpp \
  storage.cpp \
  country_info.cpp \
  country_decl.cpp \
  index.cpp \
