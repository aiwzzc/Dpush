cmake_minimum_required(VERSION 3.20)
project(Dpush)

set(CMAKE_CXX_STANDARD 20)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# 引入基础库
find_package(Protobuf REQUIRED)
find_package(gRPC CONFIG REQUIRED)
find_package(absl REQUIRED)

# 引入 gRPC 底层需要的系统库
find_package(OpenSSL REQUIRED)
find_package(ZLIB REQUIRED)
find_package(Threads REQUIRED)

set(GEN_PROTO_DIR ${CMAKE_BINARY_DIR}/proto)
set(GEN_FBS_DIR ${CMAKE_BINARY_DIR}/fbs)

file(MAKE_DIRECTORY ${GEN_PROTO_DIR})
file(MAKE_DIRECTORY ${GEN_FBS_DIR})

set(PROTO_DIR ${CMAKE_SOURCE_DIR}/proto)
file(GLOB PROTO_FILES ${PROTO_DIR}/*.proto)
set(GENERATED_PROTO_SRC "")
find_program(GRPC_CPP_PLUGIN grpc_cpp_plugin REQUIRED)

foreach(proto ${PROTO_FILES})
    get_filename_component(proto_name ${proto} NAME_WE)

    set(pb_cc ${CMAKE_BINARY_DIR}/proto/${proto_name}.pb.cc)
    set(pb_h ${CMAKE_BINARY_DIR}/proto/${proto_name}.pb.h)
    set(grpc_cc ${CMAKE_BINARY_DIR}/proto/${proto_name}.grpc.pb.cc)
    set(grpc_h ${CMAKE_BINARY_DIR}/proto/${proto_name}.grpc.pb.h)

    add_custom_command(
        OUTPUT ${pb_cc} ${grpc_cc}
        COMMAND protoc
        ARGS -I ${PROTO_DIR}
             --cpp_out=${CMAKE_BINARY_DIR}/proto
             --grpc_out=${CMAKE_BINARY_DIR}/proto
             --plugin=protoc-gen-grpc=${GRPC_CPP_PLUGIN}
             ${proto}
        DEPENDS ${proto}
        VERBATIM
    )

    list(APPEND GENERATED_PROTO_SRC ${pb_cc} ${grpc_cc})
endforeach()

add_library(proto_gen 
STATIC 
    ${GENERATED_PROTO_SRC}
)

target_include_directories(proto_gen 
PUBLIC
    ${CMAKE_BINARY_DIR}/proto
)

target_link_libraries(proto_gen 
PUBLIC
    gRPC::grpc++
    gRPC::grpc
    protobuf::libprotobuf

    absl::log_internal_check_op
    absl::log_internal_message
    absl::raw_logging_internal
)

set(FLATBUFFER_DIR ${CMAKE_SOURCE_DIR}/flatbuffers)
file(GLOB FLATBUFFER_FILES ${FLATBUFFER_DIR}/*.fbs)
set(GEN_FBS_HDRS "")

foreach(flatbuffer ${FLATBUFFER_FILES})
    get_filename_component(flatbuffer_name ${flatbuffer} NAME_WE)
    
    set(out ${CMAKE_BINARY_DIR}/fbs/${flatbuffer_name}__generated.h)

    add_custom_command(
        OUTPUT ${out}
        COMMAND flatc
        ARGS --cpp -o ${CMAKE_BINARY_DIR}/fbs ${flatbuffer}
        DEPENDS ${flatbuffer}
        VERBATIM
    )

    list(APPEND GEN_FBS_HDRS ${out})
endforeach()

add_custom_target(fbs_generate ALL DEPENDS ${GEN_FBS_HDRS})

# add_library(fbs_gen INTERFACE)

# target_include_directories(fbs_gen 
# INTERFACE
#     ${CMAKE_BINARY_DIR}/fbs
# )

# add_dependencies(fbs_gen fbs_generate)

add_subdirectory(libs)
add_subdirectory(services)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    message("Compile Debug")
    # 针对 Debug 构建类型，添加一些常见的调试相关编译选项，比如开启调试符号等
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -g")
    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -O0 -g")
else()
    message("Compile Release")
    # 针对 Release 构建类型，添加优化相关编译选项，例如开启一定级别的优化
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O3")
endif()