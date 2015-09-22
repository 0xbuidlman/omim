# Base functions tests.

TARGET = base_tests
CONFIG += console warn_on
CONFIG -= app_bundle
TEMPLATE = app

ROOT_DIR = ../..
DEPENDENCIES = base

include($$ROOT_DIR/common.pri)

win32-g++: LIBS += -lpthread

SOURCES += \
  ../../testing/testingmain.cpp \
  assert_test.cpp \
  bits_test.cpp \
  buffer_vector_test.cpp \
  cache_test.cpp \
  commands_queue_test.cpp \
  condition_test.cpp \
  const_helper.cpp \
  containers_test.cpp \
  fence_manager_test.cpp \
  logging_test.cpp \
  math_test.cpp \
  matrix_test.cpp \
  mem_trie_test.cpp \
  mru_cache_test.cpp \
  regexp_test.cpp \
  rolling_hash_test.cpp \
  scheduled_task_test.cpp \
  scope_guard_test.cpp \
  stl_add_test.cpp \
  string_format_test.cpp \
  string_utils_test.cpp \
  thread_pool_tests.cpp \
  threaded_list_test.cpp \
  threads_test.cpp \
  timer_test.cpp \
  worker_thread_test.cpp \

HEADERS +=
