cmake_minimum_required(VERSION 3.16)
project(RISC_V)

set(CMAKE_CXX_STANDARD 14)

add_executable(code src/main.cpp src/simulator.h src/memory.h src/decode.h src/alu.h src/utils.h)
