# Coding project.
TARGET = coding
TEMPLATE = lib
CONFIG += staticlib

ROOT_DIR = ..
DEPENDENCIES = base bzip2 zlib tomcrypt

include($$ROOT_DIR/common.pri)

INCLUDEPATH += ../3party/tomcrypt/src/headers ../3party/zlib

SOURCES += \
    internal/file_data.cpp \
    hex.cpp \
    file_reader.cpp \
    file_writer.cpp \
    lodepng.cpp \
    file_container.cpp \
    bzip2_compressor.cpp \
    gzip_compressor.cpp \
    timsort/timsort.cpp \
    base64.cpp \
    sha2.cpp \
    multilang_utf8_string.cpp \
    reader.cpp \
    zip_reader.cpp \
    mmap_reader.cpp \
    reader_streambuf.cpp \
    reader_writer_ops.cpp \

HEADERS += \
    internal/xmlparser.h \
    internal/expat_impl.h \
    internal/file_data.hpp \
    internal/file64_api.hpp \
    parse_xml.hpp \
    varint.hpp \
    endianness.hpp \
    byte_stream.hpp \
    var_serial_vector.hpp \
    hex.hpp \
    dd_vector.hpp \
    writer.hpp \
    write_to_sink.hpp \
    reader.hpp \
    diff.hpp \
    diff_patch_common.hpp \
    source.hpp \
    lodepng.hpp \
    lodepng_io.hpp \
    lodepng_io_private.hpp \
    var_record_reader.hpp \
    file_sort.hpp \
    file_reader.hpp \
    file_writer.hpp \
    reader_cache.hpp \
    buffer_reader.hpp \
    streams.hpp \
    streams_sink.hpp \
    streams_common.hpp \
    file_container.hpp \
    polymorph_reader.hpp \
    coder.hpp \
    coder_util.hpp \
    bzip2_compressor.hpp \
    gzip_compressor.hpp \
    timsort/timsort.hpp \
    bit_shift.hpp \
    base64.hpp \
    sha2.hpp \
    value_opt_string.hpp \
    multilang_utf8_string.hpp \
    url_encode.hpp \
    zip_reader.hpp \
    trie.hpp \
    trie_builder.hpp \
    trie_reader.hpp \
    mmap_reader.hpp \
    read_write_utils.hpp \
    file_reader_stream.hpp \
    file_writer_stream.hpp \
    reader_streambuf.hpp \
    reader_writer_ops.hpp \
    reader_wrapper.hpp \
