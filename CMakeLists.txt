cmake_minimum_required(VERSION 3.29)
project(FTP_Server)

set(CMAKE_CXX_STANDARD 20)

# Include directories
include_directories(include)

# Source files
set(SOURCES
        src/main.cpp
        src/common.cpp
        src/user_auth.cpp
        src/ftp_commands.cpp
)

# Add executable
add_executable(FTP_Server ${SOURCES})

# Link Argon2
add_library(argon2 STATIC
        argon2/src/argon2.c
        argon2/src/core.c
        argon2/src/blake2/blake2b.c
        argon2/src/thread.c
        argon2/src/encoding.c
        argon2/src/ref.c
)

target_include_directories(argon2 PUBLIC argon2/include)
target_link_libraries(FTP_Server PRIVATE argon2)
