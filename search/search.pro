# Search library.

TARGET = search
TEMPLATE = lib
CONFIG += staticlib warn_on

ROOT_DIR = ..

include($$ROOT_DIR/common.pri)

HEADERS += \
    algos.hpp \
    approximate_string_match.hpp \
    cancel_exception.hpp \
    categories_set.hpp \
    cbv.hpp \
    common.hpp \
    displayed_categories.hpp \
    downloader_search_callback.hpp \
    dummy_rank_table.hpp \
    editor_delegate.hpp \
    engine.hpp \
    everywhere_search_params.hpp \
    feature_offset_match.hpp \
    features_filter.hpp \
    features_layer.hpp \
    features_layer_matcher.hpp \
    features_layer_path_finder.hpp \
    geocoder.hpp \
    geocoder_context.hpp \
    geometry_cache.hpp \
    geometry_utils.hpp \
    hotels_classifier.hpp \
    house_detector.hpp \
    house_numbers_matcher.hpp \
    house_to_street_table.hpp \
    intermediate_result.hpp \
    intersection_result.hpp \
    interval_set.hpp \
    keyword_lang_matcher.hpp \
    keyword_matcher.hpp \
    latlon_match.hpp \
    lazy_centers_table.hpp \
    locality.hpp \
    locality_finder.hpp \
    locality_scorer.hpp \
    mode.hpp \
    model.hpp \
    mwm_context.hpp \
    nearby_points_sweeper.hpp \
    nested_rects_cache.hpp \
    pre_ranker.hpp \
    pre_ranking_info.hpp \
    processor.hpp \
    processor_factory.hpp \
    projection_on_street.hpp \
    query_params.hpp \
    query_saver.hpp \
    rank_table_cache.hpp \
    ranker.hpp \
    ranking_info.hpp \
    ranking_utils.hpp \
    region.hpp \
    result.hpp \
    retrieval.hpp \
    reverse_geocoder.hpp \
    search_index_values.hpp \
    search_params.hpp \
    search_trie.hpp \
    stats_cache.hpp \
    street_vicinity_loader.hpp \
    streets_matcher.hpp \
    string_intersection.hpp \
    suggest.hpp \
    token_slice.hpp \
    types_skipper.hpp \
    utils.hpp \
    viewport_search_callback.hpp \
    viewport_search_params.hpp

SOURCES += \
    approximate_string_match.cpp \
    cbv.cpp \
    displayed_categories.cpp \
    downloader_search_callback.cpp \
    dummy_rank_table.cpp \
    editor_delegate.cpp \
    engine.cpp \
    features_filter.cpp \
    features_layer.cpp \
    features_layer_matcher.cpp \
    features_layer_path_finder.cpp \
    geocoder.cpp \
    geocoder_context.cpp \
    geometry_cache.cpp \
    geometry_utils.cpp \
    hotels_classifier.cpp \
    house_detector.cpp \
    house_numbers_matcher.cpp \
    house_to_street_table.cpp \
    intermediate_result.cpp \
    intersection_result.cpp \
    keyword_lang_matcher.cpp \
    keyword_matcher.cpp \
    latlon_match.cpp \
    lazy_centers_table.cpp \
    locality.cpp \
    locality_finder.cpp \
    locality_scorer.cpp \
    mode.cpp \
    model.cpp \
    mwm_context.cpp \
    nearby_points_sweeper.cpp \
    nested_rects_cache.cpp \
    pre_ranker.cpp \
    pre_ranking_info.cpp \
    processor.cpp \
    projection_on_street.cpp \
    query_params.cpp \
    query_saver.cpp \
    rank_table_cache.cpp \
    ranker.cpp \
    ranking_info.cpp \
    ranking_utils.cpp \
    region.cpp \
    result.cpp \
    retrieval.cpp \
    reverse_geocoder.cpp \
    search_params.cpp \
    street_vicinity_loader.cpp \
    streets_matcher.cpp \
    token_slice.cpp \
    types_skipper.cpp \
    viewport_search_callback.cpp
