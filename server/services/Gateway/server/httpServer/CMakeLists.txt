aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR} HttpServer)

add_library(push_http ${HttpServer})

target_include_directories(push_http PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(push_http 
PUBLIC 
    muduo_net 
    muduo_base
)