add_contract(fio.system fio.oracle ${CMAKE_CURRENT_SOURCE_DIR}/fio.oracle.cpp)

target_include_directories(fio.oracle
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../fio.system/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../
        )


set_target_properties(fio.oracle
        PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
