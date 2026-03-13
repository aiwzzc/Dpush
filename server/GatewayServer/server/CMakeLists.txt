include_directories(${CMAKE_SOURCE_DIR})

aux_source_directory(${CMAKE_SOURCE_DIR}/server Server_SRC)
aux_source_directory(${CMAKE_SOURCE_DIR}/base Base_SRC)

add_executable(Gateway ../../proto/auth.grpc.pb.cc ../../proto/auth.pb.cc 
../../proto/logic.grpc.pb.cc ../../proto/logic.pb.cc 
../../proto/room.grpc.pb.cc ../../proto/room.pb.cc ${Server_SRC} ${Base_SRC})
target_link_libraries(Gateway muduo_net muduo_http jsoncpp crypto grpc++ grpc gpr protobuf absl_synchronization redis++ hiredis)
