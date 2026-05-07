aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR} CRYPTO)

add_library(push_crypto ${CRYPTO})

target_include_directories(push_crypto PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})