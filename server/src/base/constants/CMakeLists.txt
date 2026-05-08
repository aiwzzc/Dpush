aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR} CONSTANTS)

add_library(push_constants ${CONSTANTS})

target_include_directories(push_constants PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})