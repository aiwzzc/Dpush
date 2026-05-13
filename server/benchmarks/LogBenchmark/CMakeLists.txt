aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR} Bench_SRC)

add_executable(log_bench ${Bench_SRC})

target_include_directories(log_bench
PRIVATE
    ${CMAKE_SOURCE_DIR}/src/base/AeroLog
)

find_package(benchmark REQUIRED)

target_link_libraries(log_bench
PRIVATE
    push_log
    benchmark::benchmark
)