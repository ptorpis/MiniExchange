cmake_minimum_required(VERSION 3.16)
project(MiniExchangeV2 VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- Build type ---
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build type (Debug, Release, RelWithDebInfo, MinSizeRel)" FORCE)
endif()


# --- Options ---
option(ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(ENABLE_LTO "Enable Link Time Optimization (IPO/LTO)" OFF)

# --- Compiler flags ---
if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic -Wshadow)

    # Default debug flags
    set(CMAKE_CXX_FLAGS_DEBUG "-g -O0")

    # Optimized builds
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

# --- LTO (Link Time Optimization) ---
if(ENABLE_LTO)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT ipo_supported OUTPUT ipo_error)
    if(ipo_supported)
        message(STATUS "LTO (IPO) is supported and enabled")
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
    else()
        message(WARNING "LTO not supported: ${ipo_error}")
    endif()
endif()

# --- Testing ---
include(CTest)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

# --- Source files ---
file(GLOB_RECURSE ALL_SRC CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/src/*.cpp")
file(GLOB_RECURSE CLIENT_SRC CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/src/client/*.cpp")

# Remove executables and pybind11 bindings from core
list(REMOVE_ITEM ALL_SRC 
    "${CMAKE_SOURCE_DIR}/src/main.cpp"
    "${CMAKE_SOURCE_DIR}/src/py/pyClient.cpp"
)
set(CORE_SOURCES ${ALL_SRC})

# --- Core library ---
add_library(MiniExchangeCore STATIC ${CORE_SOURCES})
target_include_directories(MiniExchangeCore PUBLIC ${PROJECT_SOURCE_DIR}/include)

find_package(OpenSSL REQUIRED)
target_link_libraries(MiniExchangeCore PUBLIC OpenSSL::Crypto OpenSSL::SSL)

# --- Client library ---
add_library(MiniExchangeClient STATIC ${CLIENT_SRC})
target_include_directories(MiniExchangeClient PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(MiniExchangeClient PUBLIC MiniExchangeCore)

# --- Executable ---
add_executable(MiniExchange "${CMAKE_SOURCE_DIR}/src/main.cpp")
target_link_libraries(MiniExchange PRIVATE MiniExchangeCore)

# --- Python module ---
find_package(Python3 COMPONENTS Interpreter Development REQUIRED)
find_package(pybind11 REQUIRED)

set_target_properties(MiniExchangeCore PROPERTIES POSITION_INDEPENDENT_CODE ON)
set_target_properties(MiniExchangeClient PROPERTIES POSITION_INDEPENDENT_CODE ON)

pybind11_add_module(miniexchange_client src/py/pyClient.cpp)
target_include_directories(miniexchange_client PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(miniexchange_client PRIVATE MiniExchangeCore pybind11::module Python3::Python)
set_target_properties(miniexchange_client PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/py"
)
