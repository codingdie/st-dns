cmake_minimum_required(VERSION 3.11)
project(st-dns)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -lpthread -Wall -g")

set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -Os")


find_package(Boost 1.66.0 REQUIRED COMPONENTS system program_options thread)
find_package(OpenSSL 1.1.1 REQUIRED)
message("${OPENSSL_LIBRARIES}")

include_directories(${Boost_INCLUDE_DIR})
include_directories(${OPENSSL_INCLUDE_DIR})
message("${OPENSSL_INCLUDE_DIR}")
message("${OPENSSL_LIBRARIES}")
include_directories(src/core/dns)
include_directories(src/core/protocol/dns)
include_directories(src/core/utils)

file(GLOB_RECURSE SOURCE_FILES ./src/core/*.cpp)

message("${SOURCE_FILES}")
add_executable(st-dns src/server/main.cpp ${SOURCE_FILES})
add_executable(st-dns-test src/test/main.cpp ${SOURCE_FILES})

target_link_libraries(st-dns ${Boost_LIBRARIES} ${OPENSSL_LIBRARIES})
target_link_libraries(st-dns-test ${Boost_LIBRARIES} ${OPENSSL_LIBRARIES})

include(GNUInstallDirs)
install(TARGETS st-dns DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES st-dns.json DESTINATION ${CMAKE_INSTALL_FULL_SYSCONFDIR}/st-dns RENAME config.json)

#get_cmake_property(_variableNames VARIABLES)
#foreach (_variableName ${_variableNames})
#    message(STATUS "${_variableName}=${${_variableName}}")
#endforeach ()