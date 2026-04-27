set(CMAKE_CXX_STANDARD 20)

aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR} SRC)

set(PROTO_SRC 
${CMAKE_SOURCE_DIR}/proto/room.grpc.pb.cc 
${CMAKE_SOURCE_DIR}/proto/room.pb.cc 
${CMAKE_SOURCE_DIR}/proto/logic.grpc.pb.cc 
${CMAKE_SOURCE_DIR}/proto/logic.pb.cc 
${CMAKE_SOURCE_DIR}/proto/gateway.grpc.pb.cc 
${CMAKE_SOURCE_DIR}/proto/gateway.pb.cc
)

add_executable(LogicServer ${SRC} ${PROTO_SRC})

target_include_directories(LogicServer PUBLIC 
${CMAKE_SOURCE_DIR}/proto 
${CMAKE_SOURCE_DIR}/libs 
${CMAKE_SOURCE_DIR}/libs/concurrency 
${CMAKE_SOURCE_DIR}/libs/utils 
${CMAKE_SOURCE_DIR}/libs/tools 
${CMAKE_SOURCE_DIR}/flatbuffers 
)

target_link_libraries(LogicServer 
PRIVATE
    grpc++ 
    protobuf
    redis++ 
    hiredis 
    push_crypto 
    gpr 
    absl_synchronization
    mysqlcppconn
    push_mysqlpool
    rdkafka++ 
    rdkafka 
    push_utils
)
