cmake_minimum_required(VERSION 3.20)
project(MiniExchangeV3 VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

SET(CMAKE_EXPORT_COMPILE_COMMANDS ON)


option(WARNINGS_AS_ERRORS "Treat compiler warnings as errors" ON)

if (WARNINGS_AS_ERRORS)
    add_compile_options(-Werror)
endif()

if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(
        -Wall 
        -Wextra 
        -Wpedantic 
        -Wshadow
        -Wcast-align
        -Wunused
        -Woverloaded-virtual
        -Wconversion
        -Wsign-conversion
        -Wdouble-promotion
        -Wformat=2
        -Wimplicit-fallthrough
        -Wnull-dereference
        -Wold-style-cast
        -Wduplicated-cond
        -Wduplicated-branches
        -Wlogical-op
        -Wuseless-cast
    )

    set(CMAKE_CXX_FLAGS_DEBUG "-g -O0")

    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG -march=native")
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O2 -g -DNDEBUG -march=native")

    if(ENABLE_ASAN)
        message(STATUS "Building with AddressSanitizer")
        add_compile_options(-g -O1 -fsanitize=address -fno-omit-frame-pointer)
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address")
    else()
        message(STATUS "Building ${CMAKE_BUILD_TYPE} build")
    endif()
endif()

## add ctest here eventually

# add_subdirectory(tests)

add_library(MiniExchangeCore
    src/core/matchingEngine.cpp
    src/protocol/protocolHandler.cpp
)

target_include_directories(MiniExchangeCore PUBLIC ${CMAKE_SOURCE_DIR}/include)

add_executable(MiniExchange 
    src/main.cpp 
)
target_link_libraries(MiniExchange PRIVATE MiniExchangeCore)

