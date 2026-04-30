aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR} AsyncMysqlConnPool)

add_library(push_asyncmysqlpool ${AsyncMysqlConnPool})

target_include_directories(push_asyncmysqlpool PUBLIC 
${CMAKE_CURRENT_SOURCE_DIR} 
${CMAKE_SOURCE_DIR}/libs/concurrency
)

find_package(Boost REQUIRED COMPONENTS context)
find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)

target_link_libraries(push_asyncmysqlpool 
PUBLIC 
    Boost::boost
    Boost::context
    OpenSSL::SSL
    OpenSSL::Crypto
    Threads::Threads
)