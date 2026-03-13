aux_source_directory(${CMAKE_SOURCE_DIR}/mysql MYSQLPOOL)

add_library(MySQLPool ${MYSQLPOOL})

target_include_directories(MySQLPool PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})