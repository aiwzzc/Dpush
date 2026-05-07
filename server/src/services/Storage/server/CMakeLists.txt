aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR} SRC)

add_executable(StorageServer ${SRC})

target_include_directories(StorageServer
PUBLIC
    ${CMAKE_BINARY_DIR}/fbs
    ${CMAKE_SOURCE_DIR}/src/db
    ${CMAKE_SOURCE_DIR}/src/base
)

target_link_libraries(StorageServer
PUBLIC
    push_asyncmysqlpool
    rdkafka++ 
    rdkafka 
)
