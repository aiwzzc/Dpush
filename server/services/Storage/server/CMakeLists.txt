aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR} SRC)

add_executable(StorageServer ${SRC})

target_include_directories(StorageServer
PUBLIC
    ${CMAKE_SOURCE_DIR}/libs
    ${CMAKE_SOURCE_DIR}/utils
    ${CMAKE_BINARY_DIR}/fbs
)

target_link_libraries(StorageServer
PUBLIC
    push_asyncmysqlpool
    rdkafka++ 
    rdkafka 
    push_utils
)
