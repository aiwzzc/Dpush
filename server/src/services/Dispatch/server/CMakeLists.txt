aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR} SRC)

add_executable(DispatchServer ${SRC})

target_include_directories(DispatchServer
PRIVATE
    ${CMAKE_BINARY_DIR}/proto
    ${CMAKE_BINARY_DIR}/fbs 
    ${CMAKE_SOURCE_DIR}/src/net
    ${CMAKE_SOURCE_DIR}/src/base
    ${CMAKE_SOURCE_DIR}/src/protocol
    ${CMAKE_SOURCE_DIR}/third_party
)

target_link_libraries(DispatchServer 
PUBLIC
    etcd-cpp-api
    cpprest
    proto_gen
    redis++ 
    hiredis 
    push_yyjson
    push_http
    push_constants
    push_etcdnode
    push_fbs_proto
)