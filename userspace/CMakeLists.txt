find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBXDP REQUIRED libxdp)
pkg_check_modules(LIBBPF REQUIRED libbpf)

file(GLOB USERSPACE_SOURCES "src/*.cpp")

add_executable(userspace ${USERSPACE_SOURCES})

target_include_directories(userspace PRIVATE 
    include 
    ${LIBXDP_INCLUDE_DIRS} 
    ${LIBBPF_INCLUDE_DIRS}
)

target_link_directories(userspace PRIVATE 
    ${LIBXDP_LIBRARY_DIRS} 
    ${LIBBPF_LIBRARY_DIRS}
)

target_link_libraries(userspace PRIVATE 
    common 
    Threads::Threads 
    ${LIBXDP_LIBRARIES} 
    ${LIBBPF_LIBRARIES}
)
