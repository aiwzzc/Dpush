aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR} SRC)
aux_source_directory(${CMAKE_SOURCE_DIR}/src/base BASE)

add_executable(StorageServer ${SRC} ${BASE})

target_include_directories(StorageServer
PUBLIC
    ${CMAKE_SOURCE_DIR}/libs
    ${CMAKE_SOURCE_DIR}/utils
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
