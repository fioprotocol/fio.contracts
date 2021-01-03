add_contract(fio.templete fio.templete ${CMAKE_CURRENT_SOURCE_DIR}/fio.templete.cpp)

target_include_directories(fio.templete
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../fio.system/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../
        )

set_target_properties(fio.templete
        PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")