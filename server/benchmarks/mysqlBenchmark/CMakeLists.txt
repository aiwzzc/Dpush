aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR} SRC)

aux_source_directory(${CMAKE_SOURCE_DIR}/../libs/storage/AsyncMySQLConnPool async_mysql)
aux_source_directory(${CMAKE_SOURCE_DIR}/../libs/storage/mysql fake_async_mysql)

add_executable(mysql_bench ${SRC} ${async_mysql} ${fake_async_mysql})

target_include_directories(mysql_bench
PUBLIC
    ${CMAKE_SOURCE_DIR}/../libs/storage
    ${CMAKE_SOURCE_DIR}/../libs/concurrency
)

find_package(Boost REQUIRED COMPONENTS context)
find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)

target_link_libraries(mysql_bench
PUBLIC
    mysqlcppconn

    Boost::boost
    Boost::context
    OpenSSL::SSL
    OpenSSL::Crypto
    Threads::Threads
)