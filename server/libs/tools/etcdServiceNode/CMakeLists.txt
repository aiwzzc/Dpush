aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR} etcdNodeSRC)

add_library(push_etcdnode ${etcdNodeSRC})

target_link_libraries(push_etcdnode 
PUBLIC
    etcd-cpp-api
    cpprest
)