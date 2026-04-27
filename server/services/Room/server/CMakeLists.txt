aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR} SRC)

set(PROTO_SRC 
${CMAKE_SOURCE_DIR}/proto/room.grpc.pb.cc 
${CMAKE_SOURCE_DIR}/proto/room.pb.cc)

add_executable(RoomServer ${SRC} ${PROTO_SRC})

target_include_directories(RoomServer PUBLIC 
${CMAKE_SOURCE_DIR}/flatbuffers 
${CMAKE_SOURCE_DIR}/proto)

target_link_libraries(RoomServer 
PRIVATE
    grpc++ 
    protobuf 
    redis++ 
    hiredis 
    push_crypto 
    gpr 
    absl_synchronization
)