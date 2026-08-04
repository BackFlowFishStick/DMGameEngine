cmake_minimum_required(VERSION 3.20)
project(DMGameWorkspace
    VERSION 0.1.0
    DESCRIPTION "DMGameEngine workspace — engine + demo game"
    LANGUAGES CXX
)

# Build the engine and demo game together
add_subdirectory(engine)
add_subdirectory(game)
add_subdirectory(editor)
