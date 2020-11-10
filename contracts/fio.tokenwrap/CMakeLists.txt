add_contract(fio.tokenwrap fio.tokenwrap ${CMAKE_CURRENT_SOURCE_DIR}/teleport.cpp)

target_include_directories(fio.tokenwrap
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../
        )

set_target_properties(fio.tokenwrap
        PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
