# Base functions project.
TARGET = base
TEMPLATE = lib
CONFIG += staticlib warn_on

ROOT_DIR = ..

include($$ROOT_DIR/common.pri)

SOURCES += \
    base.cpp \
    condition.cpp \
    gmtime.cpp \
    deferred_task.cpp \
    exception.cpp \
    internal/message.cpp \
    logging.cpp \
    lower_case.cpp \
    normalize_unicode.cpp \
    object_tracker.cpp \
    shared_buffer_manager.cpp \
    src_point.cpp \
    string_format.cpp \
    string_utils.cpp \
    strings_bundle.cpp \
    sunrise_sunset.cpp \
    thread.cpp \
    thread_checker.cpp \
    thread_pool.cpp \
    threaded_container.cpp \
    timegm.cpp \
    timer.cpp \

HEADERS += \
    SRC_FIRST.hpp \
    array_adapters.hpp \
    assert.hpp \
    base.hpp \
    bits.hpp \
    buffer_vector.hpp \
    cache.hpp \
    cancellable.hpp \
    collection_cast.hpp \
    condition.hpp \
    const_helper.hpp \
    deferred_task.hpp \
    exception.hpp \
    gmtime.hpp \
    internal/messagex.hpp \
    limited_priority_queue.hpp \
    logging.hpp \
    macros.hpp \
    math.hpp \
    matrix.hpp \
    mem_trie.hpp \
    mutex.hpp \
    newtype.hpp \
    object_tracker.hpp \
    observer_list.hpp \
    range_iterator.hpp \
    ref_counted.hpp \
    regexp.hpp \
    rolling_hash.hpp \
    scope_guard.hpp \
    set_operations.hpp \
    shared_buffer_manager.hpp \
    src_point.hpp \
    stats.hpp \
    std_serialization.hpp \
    stl_add.hpp \
    stl_helpers.hpp \
    stl_iterator.hpp \
    string_format.hpp \
    string_utils.hpp \
    strings_bundle.hpp \
    sunrise_sunset.hpp \
    swap.hpp \
    thread.hpp \
    thread_checker.hpp \
    thread_pool.hpp \
    threaded_container.hpp \
    threaded_list.hpp \
    threaded_priority_queue.hpp \
    timegm.hpp \
    timer.hpp \
    worker_thread.hpp \
