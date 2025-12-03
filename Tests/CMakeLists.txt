cmake_minimum_required(VERSION 3.22)

project(MachineEQTests)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# MachineEQ Test
add_executable(Test_MachineEQ
    Test_MachineEQ.cpp
    ../Source/DSP/MachineEQ.cpp
)

target_include_directories(Test_MachineEQ PRIVATE
    ../Source
)

# Enable testing
enable_testing()
add_test(NAME MachineEQ COMMAND Test_MachineEQ)
