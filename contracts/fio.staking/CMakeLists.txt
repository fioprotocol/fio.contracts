add_contract(fio.staking fio.staking ${CMAKE_CURRENT_SOURCE_DIR}/fio.staking.cpp)

target_include_directories(fio.staking
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../fio.system/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../fio.address/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../
        ${CMAKE_CURRENT_SOURCE_DIR}/../fio.token/include
        )


set_target_properties(fio.staking
        PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
