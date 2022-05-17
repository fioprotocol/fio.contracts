add_contract(fio.asset fio.asset ${CMAKE_CURRENT_SOURCE_DIR}/src/fio.asset.cpp)

target_include_directories(fio.asset
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../fio.asset/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../
        )

set_target_properties(fio.asset
        PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
