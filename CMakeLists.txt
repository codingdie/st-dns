cmake_minimum_required(VERSION 3.11)
project(st-dns)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -lpthread -Wall -g")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -Os")

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif ()

find_package(Boost 1.66.0 REQUIRED COMPONENTS system program_options filesystem)
find_package(OpenSSL 1.1.0 REQUIRED)
message("${OPENSSL_LIBRARIES}")

include_directories(${Boost_INCLUDE_DIR})
include_directories(${OPENSSL_INCLUDE_DIR})
message("${OPENSSL_INCLUDE_DIR}")
message("${OPENSSL_LIBRARIES}")
include_directories(src/core/dns)
include_directories(src/core/whois)
include_directories(src/core/protocol/dns)
include_directories(src/core/utils)

file(GLOB_RECURSE SOURCE_FILES ./src/core/*.cpp)

message("${SOURCE_FILES}")
add_executable(st-dns src/server/main.cpp ${SOURCE_FILES})
add_executable(st-dns-test src/test/main.cpp ${SOURCE_FILES})

target_link_libraries(st-dns ${Boost_LIBRARIES} ${OPENSSL_LIBRARIES})
target_link_libraries(st-dns-test ${Boost_LIBRARIES} ${OPENSSL_LIBRARIES})

include(GNUInstallDirs)
if (OPENWRT)
    install(TARGETS st-dns DESTINATION sbin)
else (CMAKE_HOST_UNIX)
    install(TARGETS st-dns DESTINATION bin)
endif ()
install(DIRECTORY area-ips DESTINATION etc/st)

if (OPENWRT)
    install(DIRECTORY confs/openwrt/ DESTINATION etc/st/dns)
elseif (CMAKE_HOST_APPLE)
    install(DIRECTORY confs/darwin/ DESTINATION etc/st/dns)
elseif (CMAKE_HOST_UNIX)
    install(FILES confs/darwin/service/com.codingdie.st.dns.plist DESTINATION /Library/LaunchDaemons)
endif ()

#get_cmake_property(_variableNames VARIABLES)
#foreach (_variableName ${_variableNames})
#    message(STATUS "${_variableName}=${${_variableName}}")
#endforeach ()