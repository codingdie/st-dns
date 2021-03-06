cmake_minimum_required(VERSION 3.7.2)
project(st-dns)
if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif ()
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -g -lpthread")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -Os")
find_package(OpenSSL 1.1.0 REQUIRED)
find_package(Boost 1.66.0 REQUIRED COMPONENTS system filesystem thread)
include_directories(${Boost_INCLUDE_DIR})
include_directories(${OPENSSL_INCLUDE_DIR})
message("${OPENSSL_INCLUDE_DIR}")
message("${OPENSSL_LIBRARIES}")
include_directories(src/core/dns)
include_directories(src/core/whois)
include_directories(src/core/protocol/dns)
include_directories(src/core/utils)

file(GLOB_RECURSE SOURCE_FILES ./src/core/*.cpp)
file(GLOB_RECURSE SOURCE_INTEGRATION_TEST_FILES src/test/integration/*.cpp)
file(GLOB_RECURSE SOURCE_UNIT_TEST_FILES src/test/unit/*.cpp)

message("${SOURCE_FILES}")
add_executable(st-dns src/server/main.cpp ${SOURCE_FILES})

target_link_libraries(st-dns ${Boost_LIBRARIES} ${OPENSSL_LIBRARIES})

option(OPENWRT "Build for Openwrt support" OFF)
include(GNUInstallDirs)
install(TARGETS st-dns DESTINATION bin)
install(DIRECTORY area-ips DESTINATION etc/st)

if (OPENWRT)
    install(DIRECTORY confs/openwrt/ DESTINATION etc/st/dns)
elseif (APPLE)
    install(DIRECTORY confs/darwin/ DESTINATION etc/st/dns)
elseif (UNIX)
    install(DIRECTORY confs/linux/ DESTINATION etc/st/dns)
endif ()

#get_cmake_property(_variableNames VARIABLES)
#foreach (_variableName ${_variableNames})
#    message(STATUS "${_variableName}=${${_variableName}}")
#endforeach ()

if(NOT OPENWRT)
    include(FetchContent)
    FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/609281088cfefc76f9d0ce82e1ff6c30cc3591e5.zip
    )
    # For Windows: Prevent overriding the parent project's compiler/linker settings
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
    enable_testing()
    add_executable(st-dns-unit-test ${SOURCE_UNIT_TEST_FILES} ${SOURCE_FILES} ${SOURCE_COMMON_FILES})
    add_executable(st-dns-integration-test ${SOURCE_INTEGRATION_TEST_FILES} ${SOURCE_FILES} ${SOURCE_COMMON_FILES})
    target_link_libraries(st-dns-unit-test gtest_main ${Boost_LIBRARIES} ${OPENSSL_LIBRARIES})
    target_link_libraries(st-dns-integration-test gtest_main ${Boost_LIBRARIES} ${OPENSSL_LIBRARIES})
    include(GoogleTest)
    gtest_discover_tests(st-dns-unit-test)
    gtest_discover_tests(st-dns-integration-test)
endif()
