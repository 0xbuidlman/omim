# Base functions project.
TARGET = routing
TEMPLATE = lib
CONFIG += staticlib warn_on

ROOT_DIR = ..

include($$ROOT_DIR/common.pri)

DEFINES *= BOOST_ERROR_CODE_HEADER_ONLY
INCLUDEPATH += $$ROOT_DIR/3party/jansson/src \
               $$ROOT_DIR/3party/osrm/osrm-backend/Include

SOURCES += \
    astar_router.cpp \
    async_router.cpp \
    cross_routing_context.cpp \
    dijkstra_router.cpp \
    features_road_graph.cpp \
    osrm2feature_map.cpp \
    osrm_online_router.cpp \
    osrm_router.cpp \
    road_graph.cpp \
    road_graph_router.cpp \
    route.cpp \
    routing_mapping.cpp \
    turns.cpp \
    vehicle_model.cpp \


HEADERS += \
    astar_router.hpp \
    async_router.hpp \
    cross_routing_context.hpp \
    dijkstra_router.hpp \
    features_road_graph.hpp \
    osrm2feature_map.hpp \
    osrm_data_facade.hpp \
    osrm_online_router.hpp \
    osrm_router.hpp \
    road_graph.hpp \
    road_graph_router.hpp \
    route.hpp \
    router.hpp \
    routing_mapping.h \
    turns.hpp \
    vehicle_model.hpp \
