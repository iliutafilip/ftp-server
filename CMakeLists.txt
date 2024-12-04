cmake_minimum_required(VERSION 3.29)
project(FTP_Server)

set(CMAKE_CXX_STANDARD 20)

# Add Argon2 as a static library
add_library(argon2 STATIC
        argon2/src/argon2.c
        argon2/src/core.c
        argon2/src/blake2/blake2b.c
        argon2/src/thread.c
        argon2/src/encoding.c
        argon2/src/ref.c
        include/common.h
)

# Include Argon2 headers
target_include_directories(argon2 PUBLIC argon2/include)

# Add the main executable
add_executable(FTP_Server main.cpp
        include/common.h)

# Link the Argon2 library to your executable
target_link_libraries(FTP_Server PRIVATE argon2)
