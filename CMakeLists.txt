cmake_minimum_required(VERSION 3.20)
project(m2-bot LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT MSVC)
    message(FATAL_ERROR "Bu proje MSVC (Visual Studio) ile derlenmelidir.")
endif()

add_subdirectory(injector)
add_subdirectory(payload)
