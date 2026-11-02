aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR} Bench_SRC)

add_executable(broadcast_bench ${Bench_SRC})

target_include_directories(broadcast_bench
PRIVATE
    ${CMAKE_SOURCE_DIR}/third_party
    ${CMAKE_BINARY_DIR}/fbs 
)

target_link_libraries(broadcast_bench
PRIVATE
    muduo_net
)