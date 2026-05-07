cmake_minimum_required(VERSION 3.20)
project(LogicBenchmark)

set(CMAKE_CXX_STANDARD 20)

include_directories(/home/zzc/linux_test/DistributedPush/server/GatewayServer)
link_directories(/home/zzc/linux_test/DistributedPush/server/GatewayServer/build/lib) 

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_executable(logicBenchmark MaxConnBenchmark.cc)

target_link_libraries(logicBenchmark muduo_net muduo_base)