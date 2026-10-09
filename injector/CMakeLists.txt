cmake_minimum_required(VERSION 3.20)
project(M2Injector LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)

add_executable(M2Injector src/main.cpp)

target_link_libraries(M2Injector PRIVATE
    kernel32
    user32
    shell32
    advapi32
    comdlg32
)

set_target_properties(M2Injector PROPERTIES
    OUTPUT_NAME "M2Injector"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
