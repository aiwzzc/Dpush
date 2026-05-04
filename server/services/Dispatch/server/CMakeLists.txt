aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR} SRC)

add_executable(DispatchServer ${SRC})

target_include_directories(DispatchServer
PRIVATE
    ${CMAKE_SOURCE_DIR}/libs/tools 
    ${PROJECT_SOURCE_DIR}
    ${CMAKE_BINARY_DIR}/proto
    ${CMAKE_SOURCE_DIR}/libs/concurrency
)

target_link_libraries(DispatchServer 
PUBLIC
    etcd-cpp-api
    cpprest
    redis++ 
    hiredis 
    push_yyjson
    push_http
)