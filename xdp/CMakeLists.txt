find_program(CLANG_EXE clang REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBXDP REQUIRED libxdp)

set(XDP_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src/xdp.c")
set(XDP_OBJ "${CMAKE_CURRENT_BINARY_DIR}/xdp.o")

add_custom_command(
    OUTPUT ${XDP_OBJ}
    COMMAND ${CLANG_EXE} -O2 -g -target bpf -I${CMAKE_CURRENT_SOURCE_DIR}/include -c ${XDP_SRC} -o ${XDP_OBJ}
    DEPENDS ${XDP_SRC}
    VERBATIM
)

add_custom_target(xdp_bpf ALL DEPENDS ${XDP_OBJ})

add_executable(xdp_loader src/loader.c)

target_include_directories(xdp_loader PRIVATE ${LIBXDP_INCLUDE_DIRS} include)
target_link_directories(xdp_loader PRIVATE ${LIBXDP_LIBRARY_DIRS})
target_link_libraries(xdp_loader PRIVATE ${LIBXDP_LIBRARIES})

add_dependencies(xdp_loader xdp_bpf)
