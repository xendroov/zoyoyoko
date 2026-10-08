cmake_minimum_required(VERSION 3.20)
project(KoxpPayload LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)

# WebView2 SDK - NuGet'ten veya manuel olarak indirilmeli
# set(WEBVIEW2_DIR "${CMAKE_SOURCE_DIR}/packages/Microsoft.Web.WebView2")

file(GLOB_RECURSE SOURCES
    "src/*.cpp"
)

file(GLOB_RECURSE HEADERS
    "include/*.h"
)

add_library(KoxpPayload SHARED ${SOURCES} ${HEADERS})

target_include_directories(KoxpPayload PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    # ${WEBVIEW2_DIR}/build/native/include
)

target_link_libraries(KoxpPayload PRIVATE
    kernel32
    user32
    gdi32
    shell32
    advapi32
    # ${WEBVIEW2_DIR}/build/native/x86/WebView2LoaderStatic.lib
)

# UI dosyasini build dizinine kopyala
add_custom_command(TARGET KoxpPayload POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/ui/index.html"
        "$<TARGET_FILE_DIR:KoxpPayload>/ui/index.html"
)

set_target_properties(KoxpPayload PROPERTIES
    OUTPUT_NAME "KoxpPayload"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
