cmake_minimum_required(VERSION 3.11)
project(st_dns)

set(CMAKE_CXX_STANDARD 11)
#set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -Os")


find_package(Boost 1.66.0 REQUIRED COMPONENTS system program_options)
include_directories(${Boost_INCLUDE_DIR})

add_executable(st-dns src/main.cpp src/Config.cpp src/Config.h src/LogUtils.cpp src/LogUtils.h src/DNSServer.cpp src/DNSServer.h)
target_link_libraries(st-dns ${Boost_LIBRARIES})

include(GNUInstallDirs)
install(TARGETS st-dns DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES st-dns.json DESTINATION ${CMAKE_INSTALL_FULL_SYSCONFDIR}/st-dns RENAME config.json)
