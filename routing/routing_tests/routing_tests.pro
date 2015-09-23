 # Routing lib unit tests

TARGET = routing_tests
CONFIG += console warn_on
CONFIG -= app_bundle
TEMPLATE = app

ROOT_DIR = ../..
DEPENDENCIES = routing indexer platform geometry coding base osrm protobuf tomcrypt succinct

macx-*: LIBS *= "-framework Foundation" "-framework IOKit"

include($$ROOT_DIR/common.pri)

QT *= core

win32* : LIBS *= -lShell32


SOURCES += \
  ../../testing/testingmain.cpp \
  cross_routing_tests.cpp \
  dijkstra_router_test.cpp \
  features_road_graph_test.cpp \
  osrm_router_test.cpp \
  road_graph_builder.cpp \
  vehicle_model_test.cpp \

HEADERS += \
  features_road_graph_test.hpp \
  road_graph_builder.hpp \
