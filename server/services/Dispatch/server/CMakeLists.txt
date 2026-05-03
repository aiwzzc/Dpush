aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR} SRC)

add_executable(DispatchServer ${SRC})

target_include_directories(DispatchServer
PUBLIC
    ${CMAKE_SOURCE_DIR}/libs/tools 
)

target_link_libraries(DispatchServer 
PUBLIC
    etcd-cpp-api
    cpprest
    redis++ 
    hiredis 
    push_yyjson
)