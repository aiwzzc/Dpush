
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR} SRC)
set(PROTO_SRC 
${CMAKE_SOURCE_DIR}/proto/auth.grpc.pb.cc 
${CMAKE_SOURCE_DIR}/proto/auth.pb.cc)

add_executable(AuthServer ${SRC} ${PROTO_SRC})

target_include_directories(AuthServer PUBLIC 
${CMAKE_SOURCE_DIR}/libs  
${CMAKE_SOURCE_DIR}/proto
)

target_link_libraries(AuthServer 
PRIVATE  
    mysqlcppconn 
    push_mysqlpool 
    push_crypto 
    grpc++ 
    protobuf 
    uuid 
    redis++ 
    hiredis 
    gpr 
    absl_synchronization 
    jwt
)