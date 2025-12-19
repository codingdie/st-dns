cmake_minimum_required(VERSION 3.11)
project(st-dns)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -lpthread -latomic -Wall -g")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -Os")

if (NOT EXISTS ${PROJECT_SOURCE_DIR}/src/common/main/thirdpart/httplib.h)
    file(DOWNLOAD "https://raw.githubusercontent.com/yhirose/cpp-httplib/v0.12.6/httplib.h" ${PROJECT_SOURCE_DIR}/src/common/main/thirdpart/httplib.h)
endif ()

find_package(OpenSSL 1.0.2 REQUIRED)
find_package(Boost 1.66.0 REQUIRED COMPONENTS system filesystem thread program_options log log_setup)
find_package(leveldb REQUIRED)
find_package(Protobuf REQUIRED)
find_program(PROTOC protoc REQUIRED)
message("protobuf:${Protobuf_LITE_LIBRARY}")
message("protobuf:${Protobuf_LIBRARY}")
message("protoc: ${PROTOC}")
#protobuf
execute_process(COMMAND rm -f ${CMAKE_CURRENT_LIST_DIR}/src/core/protocol/*.pb.* ${CMAKE_CURRENT_LIST_DIR}/src/common/main/proto/*.pb.*)
execute_process(COMMAND ${PROTOC} -I=${CMAKE_CURRENT_LIST_DIR}/src/core/protocol --cpp_out=${CMAKE_CURRENT_LIST_DIR}/src/core/protocol message.proto
    OUTPUT_VARIABLE protoc_output_message
    ERROR_VARIABLE protoc_error_message
    RESULT_VARIABLE protoc_result_message)
if(protoc_result_message EQUAL 0)
    message("Generated protobuf files: ${CMAKE_CURRENT_LIST_DIR}/src/core/protocol/message.pb.h ${CMAKE_CURRENT_LIST_DIR}/src/core/protocol/message.pb.cc")
else()
    message("Error: Protobuf generation for message.proto failed with result ${protoc_result_message}")
    message("Output: ${protoc_output_message}")
    message("Error: ${protoc_error_message}")
endif()
execute_process(COMMAND ${PROTOC} -I=${CMAKE_CURRENT_LIST_DIR}/src/common/main/proto --cpp_out=${CMAKE_CURRENT_LIST_DIR}/src/common/main/proto kv.proto
    OUTPUT_VARIABLE protoc_output
    ERROR_VARIABLE protoc_error
    RESULT_VARIABLE protoc_result)
if(protoc_result EQUAL 0)
    message("Generated protobuf files: ${CMAKE_CURRENT_LIST_DIR}/src/common/main/proto/kv.pb.h ${CMAKE_CURRENT_LIST_DIR}/src/common/main/proto/kv.pb.cc")
else()
    message("Error: Protobuf generation for kv.proto failed with result ${protoc_result}")
    message("Output: ${protoc_output}")
    message("Error: ${protoc_error}")
endif()

#include
include_directories(src/core)
include_directories(src/common/main)
include_directories(src/test)
IF (APPLE)
    include_directories(src/pf/10.15.6)
ENDIF ()


#st-dns target
file(GLOB_RECURSE SOURCE_FILES src/core/*.cpp src/core/*.cc)
file(GLOB_RECURSE SOURCE_COMMON_FILES src/common/main/*.cpp src/common/main/*.cc)
file(GLOB_RECURSE SOURCE_INTEGRATION_TEST_FILES src/test/integration/*.cpp)
file(GLOB_RECURSE SOURCE_UNIT_TEST_FILES src/test/unit/*.cpp)
file(GLOB_RECURSE SOURCE_ST_UNIT_TEST_FILES src/common/test/*.cpp)

add_executable(st-dns src/server/main.cpp ${SOURCE_FILES} ${SOURCE_COMMON_FILES})
set(OTHER_LIBRARIES "rt" "leveldb::leveldb")
target_link_libraries(st-dns ${Boost_LIBRARIES} ${OPENSSL_LIBRARIES} ${OTHER_LIBRARIES} ${Protobuf_LITE_LIBRARY})


#install
include(GNUInstallDirs)
install(TARGETS st-dns DESTINATION bin)
option(OPENWRT "Build for Openwrt support" OFF)

if (OPENWRT)
    install(DIRECTORY confs/openwrt/ DESTINATION etc/st/dns)
elseif (CMAKE_HOST_APPLE)
    install(DIRECTORY confs/darwin/ DESTINATION etc/st/dns)
elseif (CMAKE_HOST_UNIX)
    install(DIRECTORY confs/linux/ DESTINATION etc/st/dns)
endif ()

#gtest
if (NOT OPENWRT)
    include(FetchContent)
    FetchContent_Declare(
            googletest
            URL https://github.com/google/googletest/archive/609281088cfefc76f9d0ce82e1ff6c30cc3591e5.zip
    )
    # For Windows: Prevent overriding the parent project's compiler/linker settings
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
    enable_testing()
    add_executable(st-unit-test ${SOURCE_ST_UNIT_TEST_FILES} ${SOURCE_COMMON_FILES})
    add_executable(st-dns-unit-test ${SOURCE_UNIT_TEST_FILES} ${SOURCE_FILES} ${SOURCE_COMMON_FILES})
    add_executable(st-dns-integration-test ${SOURCE_INTEGRATION_TEST_FILES} ${SOURCE_FILES} ${SOURCE_COMMON_FILES})
    target_link_libraries(st-unit-test gtest_main ${Boost_LIBRARIES} ${OPENSSL_LIBRARIES} ${OTHER_LIBRARIES} ${Protobuf_LITE_LIBRARY})
    target_link_libraries(st-dns-unit-test gtest_main ${Boost_LIBRARIES} ${OPENSSL_LIBRARIES} ${OTHER_LIBRARIES} ${Protobuf_LITE_LIBRARY})
    target_link_libraries(st-dns-integration-test gtest_main ${Boost_LIBRARIES} ${OPENSSL_LIBRARIES} ${OTHER_LIBRARIES} ${Protobuf_LITE_LIBRARY})

    include(GoogleTest)
    gtest_discover_tests(st-unit-test)
    gtest_discover_tests(st-dns-unit-test)
    gtest_discover_tests(st-dns-integration-test)
endif ()
