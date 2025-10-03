cmake_minimum_required(VERSION 3.16)
project(MiniExchangeV2 VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- Build type (default Debug) ---
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

# --- Option to enable AddressSanitizer ---
option(ENABLE_ASAN "Enable AddressSanitizer" OFF)

# --- Compiler flags ---
if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic -Wshadow)
    if(ENABLE_ASAN)
        message(STATUS "Building with AddressSanitizer")
        add_compile_options(-g -O1 -fsanitize=address -fno-omit-frame-pointer)
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address")
    else()
        message(STATUS "Building regular Debug")
        add_compile_options(-g -O0)
    endif()
endif()

include(CTest)
if(BUILD_TESTING)
   add_subdirectory(tests)
endif()

# --- Source files ---
file(GLOB_RECURSE ALL_SRC CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/src/*.cpp")
file(GLOB_RECURSE CLIENT_SRC CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/src/client/*.cpp")
list(REMOVE_ITEM ALL_SRC 
"${CMAKE_SOURCE_DIR}/src/main.cpp"
"${CMAKE_SOURCE_DIR}/src/client_main.cpp"
)
set(CORE_SOURCES ${ALL_SRC})

# --- Core library ---
add_library(MiniExchangeCore STATIC ${CORE_SOURCES})
add_library(MiniExchangeClient STATIC ${CLIENT_SRC})
target_include_directories(MiniExchangeClient
    PUBLIC ${PROJECT_SOURCE_DIR}/include
)
target_link_libraries(MiniExchangeClient PUBLIC MiniExchangeCore)

target_include_directories(MiniExchangeCore
    PUBLIC ${PROJECT_SOURCE_DIR}/include
)

find_package(OpenSSL REQUIRED)
target_link_libraries(MiniExchangeCore PUBLIC OpenSSL::Crypto OpenSSL::SSL)

# --- Executable ---
add_executable(MiniExchange "${CMAKE_SOURCE_DIR}/src/main.cpp")
target_link_libraries(MiniExchange PRIVATE MiniExchangeCore)

add_executable(ClientBin "${CMAKE_SOURCE_DIR}/src/client_main.cpp")
target_link_libraries(ClientBin PRIVATE MiniExchangeCore)
