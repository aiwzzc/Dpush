aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR} CONFIG_BUILER_SRC)

add_library(push_config_builder ${CONFIG_BUILER_SRC})

target_include_directories(push_config_builder
PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)