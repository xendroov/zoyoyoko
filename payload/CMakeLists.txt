cmake_minimum_required(VERSION 3.20)
project(M2Payload LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)

# WebView2 SDK
set(WEBVIEW2_DIR "${CMAKE_SOURCE_DIR}/packages")

file(GLOB_RECURSE SOURCES
    "src/*.cpp"
)

file(GLOB_RECURSE HEADERS
    "include/*.h"
)

add_library(M2Payload SHARED ${SOURCES} ${HEADERS})

target_include_directories(M2Payload PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${WEBVIEW2_DIR}/build/native/include
)

target_link_libraries(M2Payload PRIVATE
    kernel32
    user32
    gdi32
    shell32
    advapi32
    ole32
    shlwapi
    ${WEBVIEW2_DIR}/build/native/x86/WebView2LoaderStatic.lib
)

# UI dosyasini build dizinine kopyala
add_custom_command(TARGET M2Payload POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/ui/index.html"
        "$<TARGET_FILE_DIR:M2Payload>/ui/index.html"
)

set_target_properties(M2Payload PROPERTIES
    OUTPUT_NAME "M2Payload"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
