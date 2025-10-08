include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION OFF)
FetchContent_MakeAvailable(googletest)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE) # quick fix for dev warning

add_executable(UnitTests
    engineTests.cpp
    protocolTests.cpp
)

target_link_libraries(UnitTests
    PRIVATE
        MiniExchangeCore
        MiniExchangeClient
        GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(UnitTests)
