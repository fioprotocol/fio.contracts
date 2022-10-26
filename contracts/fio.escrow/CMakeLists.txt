add_contract(fio.escrow fio.escrow ${CMAKE_CURRENT_SOURCE_DIR}/fio.escrow.cpp)

target_include_directories(fio.escrow
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/../
        ${CMAKE_CURRENT_SOURCE_DIR}/../fio.system/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../fio.address/include
        )

set_target_properties(fio.escrow
        PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")