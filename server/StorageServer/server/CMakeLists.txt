include_directories(${CMAKE_SOURCE_DIR})

aux_source_directory(${CMAKE_SOURCE_DIR}/server Server_SRC)

add_executable(StorageServer ${Server_SRC})
target_link_libraries(StorageServer jsoncpp crypto grpc++ grpc gpr protobuf absl_synchronization redis++ hiredis rdkafka++ rdkafka)
