cmake_minimum_required(VERSION 3.15)
project(FTPclient C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pthread")

add_executable(FTPclient main.c ftpklient.h)