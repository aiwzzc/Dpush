aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR} MYSQLPOOL)

add_library(push_mysqlpool ${MYSQLPOOL})

target_include_directories(push_mysqlpool PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(push_mysqlpool 
PUBLIC 
    mysqlcppconn
)