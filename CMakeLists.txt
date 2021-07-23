cmake_minimum_required(VERSION 3.17)
project(MineNet)

set(CMAKE_CXX_STANDARD 20)

INCLUDE_DIRECTORIES(libs/asio-1.18.2/include)

add_library(MineNet MineNet.cpp MineNet.h MineNetMessage.hpp MineThreadSafeQueue.hpp MineNetClient.hpp MineNetConnection.hpp MineNetServer.hpp)

target_link_libraries(MineNet -lwsock32 ws2_32.lib)