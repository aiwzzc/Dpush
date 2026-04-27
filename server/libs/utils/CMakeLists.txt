aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR} UTILS)

add_library(push_utils ${UTILS})

target_include_directories(push_utils PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})