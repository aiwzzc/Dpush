include_directories(${CMAKE_SOURCE_DIR})

aux_source_directory(${CMAKE_SOURCE_DIR}/server Server_SRC)

add_executable(RoomServer ../../proto/room.grpc.pb.cc ../../proto/room.pb.cc  ${Server_SRC})
target_link_libraries(RoomServer grpc++ protobuf redis++ hiredis gpr absl_synchronization)