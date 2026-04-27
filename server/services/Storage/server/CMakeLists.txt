aux_source_directory(${CMAKE_SOURCE_DIR}/server SRC)

add_executable(Storage ${SRC})

target_link_libraries(Storage jsoncpp crypto grpc++ grpc gpr protobuf 
absl_synchronization redis++ hiredis rdkafka++ rdkafka)
