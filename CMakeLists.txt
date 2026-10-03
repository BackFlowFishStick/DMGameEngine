cmake_minimum_required(VERSION 3.20)
project(DMGameWorkspace
    VERSION 0.1.0
    DESCRIPTION "DMGameEngine workspace — engine + demo game"
    LANGUAGES CXX
)

# Testing must be enabled at the top level so the build root gets a
# CTestTestfile.cmake — otherwise `ctest --test-dir <build>` reports
# "No tests were found" (see kb/KB-07 K-011). add_test()/gtest_discover_tests
# calls in engine/tests still work: enable_testing() covers this directory and
# all subdirectories.
enable_testing()

# Build the engine and demo game together
add_subdirectory(engine)
add_subdirectory(game)
add_subdirectory(editor)
