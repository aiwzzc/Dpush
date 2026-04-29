aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR} SRC)

set(PROTO_SRC 
${CMAKE_SOURCE_DIR}/proto/room.grpc.pb.cc 
${CMAKE_SOURCE_DIR}/proto/room.pb.cc)

add_executable(RoomServer ${SRC})

# target_include_directories(RoomServer PUBLIC 
# ${CMAKE_SOURCE_DIR}/flatbuffers 
# ${CMAKE_SOURCE_DIR}/proto)

target_include_directories(RoomServer PUBLIC 
${CMAKE_BINARY_DIR}/fbs 
${CMAKE_BINARY_DIR}/proto)

set(GRPC_LIBS
    grpc++
    grpc
    gpr
    protobuf 
    absl_synchronization 
    absl_base
)

target_link_libraries(RoomServer 
PRIVATE
    ${GRPC_LIBS} 
    OpenSSL::SSL
    OpenSSL::Crypto
    ZLIB::ZLIB
    Threads::Threads

    redis++ 
    hiredis 
    push_crypto 
    proto_gen
)