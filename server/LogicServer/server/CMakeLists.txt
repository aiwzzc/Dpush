include_directories(${CMAKE_SOURCE_DIR})

set(CMAKE_CXX_STANDARD 20)

aux_source_directory(${CMAKE_SOURCE_DIR}/server Server_SRC)
aux_source_directory(${CMAKE_SOURCE_DIR}/../AuthServer/mysql mysql_SRC)

add_executable(LogicServer ../../proto/room.grpc.pb.cc ../../proto/room.pb.cc 
../../proto/logic.grpc.pb.cc ../../proto/logic.pb.cc ${Server_SRC} ${mysql_SRC})
target_link_libraries(LogicServer
    grpc++ protobuf
    redis++ hiredis
    gpr absl_synchronization jsoncpp
    mysqlcppconn
    rdkafka++ rdkafka
)
