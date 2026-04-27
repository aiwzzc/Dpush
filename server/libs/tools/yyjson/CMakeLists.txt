aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR} YYJSON)

add_library(push_yyjson ${YYJSON})

target_include_directories(push_yyjson PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})