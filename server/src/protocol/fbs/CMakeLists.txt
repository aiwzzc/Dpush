aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR} FBS_PROTOCOL)

add_library(push_fbs_proto ${FBS_PROTOCOL})

target_include_directories(push_fbs_proto 
PUBLIC
    ${CMAKE_BINARY_DIR}/fbs
)