include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(core_tests
    # test cpp files come here
    core_tests.cpp
)

target_link_libraries(core_tests
    PRIVATE
        MiniExchangeCore
        GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(core_tests)
