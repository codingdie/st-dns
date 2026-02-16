cmake_minimum_required(VERSION 3.11)
project(st-dns)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -lpthread -latomic -Wall -g")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -Os")

if (NOT EXISTS ${PROJECT_SOURCE_DIR}/src/common/main/thirdpart/httplib.h)
    file(DOWNLOAD "https://raw.githubusercontent.com/yhirose/cpp-httplib/v0.12.6/httplib.h" ${PROJECT_SOURCE_DIR}/src/common/main/thirdpart/httplib.h)
endif ()

find_package(OpenSSL 1.0.2 REQUIRED)
find_package(Boost 1.7.0 REQUIRED COMPONENTS system filesystem thread program_options log log_setup)
find_package(leveldb REQUIRED)
find_package(Protobuf REQUIRED)

# Protobuf code generation
protobuf_generate_cpp(MESSAGE_PROTO_SRCS MESSAGE_PROTO_HDRS src/core/protocol/message.proto)
protobuf_generate_cpp(KV_PROTO_SRCS KV_PROTO_HDRS src/common/main/proto/kv.proto)

# Include directories
include_directories(src/core)
include_directories(src/common/main)
include_directories(src/test)
include_directories(${CMAKE_CURRENT_BINARY_DIR})
if(APPLE)
    include_directories(src/pf/10.15.6)
endif()

# Source files
file(GLOB_RECURSE SOURCE_FILES src/core/*.cpp src/core/*.cc)
file(GLOB_RECURSE SOURCE_COMMON_FILES src/common/main/*.cpp src/common/main/*.cc)

# Main executable
add_executable(st-dns src/server/main.cpp ${SOURCE_FILES} ${SOURCE_COMMON_FILES} ${MESSAGE_PROTO_SRCS} ${KV_PROTO_SRCS})
set(OTHER_LIBRARIES "rt" "leveldb::leveldb")
target_link_libraries(st-dns ${Boost_LIBRARIES} ${OPENSSL_LIBRARIES} ${OTHER_LIBRARIES} ${Protobuf_LITE_LIBRARY})

# Install
include(GNUInstallDirs)
install(TARGETS st-dns DESTINATION bin)
option(OPENWRT "Build for Openwrt support" OFF)

if(OPENWRT)
    install(DIRECTORY confs/openwrt/ DESTINATION etc/st/dns)
elseif(CMAKE_HOST_APPLE)
    install(DIRECTORY confs/darwin/ DESTINATION etc/st/dns)
elseif(CMAKE_HOST_UNIX)
    install(DIRECTORY confs/linux/ DESTINATION etc/st/dns)
endif()

# Tests (disabled for OpenWrt)
if(NOT OPENWRT)
    file(GLOB_RECURSE SOURCE_INTEGRATION_TEST_FILES src/test/integration/*.cpp)
    file(GLOB_RECURSE SOURCE_UNIT_TEST_FILES src/test/unit/*.cpp)
    file(GLOB_RECURSE SOURCE_ST_UNIT_TEST_FILES src/common/test/*.cpp)

    include(FetchContent)
    FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/609281088cfefc76f9d0ce82e1ff6c30cc3591e5.zip
    )
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    enable_testing()
    add_executable(st-unit-test ${SOURCE_ST_UNIT_TEST_FILES} ${SOURCE_COMMON_FILES} ${MESSAGE_PROTO_SRCS} ${KV_PROTO_SRCS})
    add_executable(st-dns-unit-test ${SOURCE_UNIT_TEST_FILES} ${SOURCE_FILES} ${SOURCE_COMMON_FILES} ${MESSAGE_PROTO_SRCS} ${KV_PROTO_SRCS})
    add_executable(st-dns-integration-test ${SOURCE_INTEGRATION_TEST_FILES} ${SOURCE_FILES} ${SOURCE_COMMON_FILES} ${MESSAGE_PROTO_SRCS} ${KV_PROTO_SRCS})

    target_link_libraries(st-unit-test gtest_main ${Boost_LIBRARIES} ${OPENSSL_LIBRARIES} ${OTHER_LIBRARIES} ${Protobuf_LITE_LIBRARY})
    target_link_libraries(st-dns-unit-test gtest_main ${Boost_LIBRARIES} ${OPENSSL_LIBRARIES} ${OTHER_LIBRARIES} ${Protobuf_LITE_LIBRARY})
    target_link_libraries(st-dns-integration-test gtest_main ${Boost_LIBRARIES} ${OPENSSL_LIBRARIES} ${OTHER_LIBRARIES} ${Protobuf_LITE_LIBRARY})

    include(GoogleTest)
    gtest_discover_tests(st-unit-test PROPERTIES  RESOURCE_LOCK dns_db TIMEOUT 120)
    gtest_discover_tests(st-dns-unit-test PROPERTIES RESOURCE_LOCK dns_db TIMEOUT 120)
    gtest_discover_tests(st-dns-integration-test PROPERTIES RESOURCE_LOCK dns_db TIMEOUT 120)
endif()
