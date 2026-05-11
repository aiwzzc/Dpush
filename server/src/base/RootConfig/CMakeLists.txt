aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR} CONFIG_SRC)

add_library(push_config ${CONFIG_SRC})

target_include_directories(push_config PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(push_config
PUBLIC
    yaml-cpp
)