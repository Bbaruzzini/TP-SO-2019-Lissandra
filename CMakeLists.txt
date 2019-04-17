cmake_minimum_required(VERSION 3.5)
project(Lissandra C)

set(CMAKE_C_STANDARD 11)

include_directories(Shared)
include_directories(Shared/Config)
include_directories(Shared/Console)
include_directories(Shared/Logging)
include_directories(Shared/Networking)
include_directories(Shared/Protocol)
include_directories(Shared/Utility)

file(GLOB_RECURSE SHARED_SRCS Shared/*.c Shared/*.h)
add_library(shared STATIC ${SHARED_SRCS} LFS/Lissandra.c LFS/Lissandra.h)

file(GLOB KERNEL_SRCS Kernel/*.c Kernel/*.h)
add_executable(Kernel ${KERNEL_SRCS})

# dame warnings baby
add_library(warnings-interface INTERFACE LFS/Lissandra.c LFS/Lissandra.h)
target_compile_options(warnings-interface
        INTERFACE
        -Wall
        -Werror
        -Wextra
        -Wstrict-prototypes
        -pedantic)

target_link_libraries(shared pthread readline warnings-interface)
target_link_libraries(Kernel shared warnings-interface)
