
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR} SRC)
set(PROTO_SRC 
${CMAKE_SOURCE_DIR}/proto/auth.grpc.pb.cc 
${CMAKE_SOURCE_DIR}/proto/auth.pb.cc)

add_executable(AuthServer ${SRC})

target_include_directories(AuthServer PUBLIC 
${CMAKE_SOURCE_DIR}/libs  
${CMAKE_BINARY_DIR}/proto
)

find_package(utf8_range REQUIRED)

set(GRPC_LIBS
    grpc++
    grpc
    gpr
    protobuf 
    absl_synchronization 
    absl_base
)

target_link_libraries(AuthServer 
PRIVATE  
    mysqlcppconn 
    push_mysqlpool 
    push_crypto 

    ${GRPC_LIBS} 
    OpenSSL::SSL
    OpenSSL::Crypto
    ZLIB::ZLIB
    Threads::Threads

    absl::synchronization
    absl::base
    absl::strings
    utf8_range::utf8_range

    uuid 
    redis++ 
    hiredis  
    jwt 
    proto_gen
)