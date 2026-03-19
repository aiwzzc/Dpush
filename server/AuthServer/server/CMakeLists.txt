include_directories(${CMAKE_SOURCE_DIR})

aux_source_directory(${CMAKE_SOURCE_DIR}/server Server_SRC)
aux_source_directory(${CMAKE_SOURCE_DIR}/mysql MySQLPool_SRC)
aux_source_directory(${CMAKE_SOURCE_DIR}/base Base_SRC)

add_executable(AuthServer ../../proto/auth.grpc.pb.cc ../../proto/auth.pb.cc  ${Server_SRC} ${MySQLPool_SRC} ${Base_SRC})
target_link_libraries(AuthServer jsoncpp mysqlcppconn MySQLPool grpc++ protobuf uuid redis++ hiredis gpr absl_synchronization jwt)