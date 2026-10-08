cmake_minimum_required(VERSION 3.20)
project(KoxpInjector LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)

add_executable(KoxpInjector src/main.cpp)

target_link_libraries(KoxpInjector PRIVATE
    kernel32
    user32
    shell32
    advapi32
    comdlg32
)

set_target_properties(KoxpInjector PROPERTIES
    OUTPUT_NAME "KoxpInjector"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
