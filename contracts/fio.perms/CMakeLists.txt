add_contract(fio.perms fio.perms ${CMAKE_CURRENT_SOURCE_DIR}/fio.perms.cpp)

target_include_directories(fio.perms
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../fio.perms/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../fio.address/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../
        )


set_target_properties(fio.perms
        PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
