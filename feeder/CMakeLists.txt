add_executable(main src/main.cpp src/packet_util.cpp)

target_include_directories(main PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
