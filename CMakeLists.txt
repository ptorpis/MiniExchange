cmake_minimum_required(VERSION 3.16)
project(MiniExchangeV2 VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_BUILD_TYPE "Debug")

if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic -Wshadow -g -O0)
endif()

file(GLOB_RECURSE ALL_SRC CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/src/*.cpp")

list(REMOVE_ITEM ALL_SRC
"${CMAKE_SOURCE_DIR}/src/main.cpp"
)

set(CORE_SOURCES ${ALL_SRC})

add_library(MiniExchangeCore STATIC ${CORE_SOURCES})

target_include_directories(MiniExchangeCore
    PUBLIC ${PROJECT_SOURCE_DIR}/include
)

find_package(OpenSSL REQUIRED)

target_link_libraries(MiniExchangeCore PUBLIC OpenSSL::Crypto OpenSSL::SSL)

add_executable(MiniExchange "${CMAKE_SOURCE_DIR}/src/main.cpp")
target_link_libraries(MiniExchange PRIVATE MiniExchangeCore)