cmake_minimum_required(VERSION 3.6)
project(LatencyHunter)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(SENDER_FILES sender.c)
add_executable(sender ${SENDER_FILES})

set(RECEIVER_FILES receiver.c)
add_executable(receiver ${RECEIVER_FILES})