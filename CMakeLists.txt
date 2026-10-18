cmake_minimum_required(VERSION 3.15)
project(keep-awake VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(keep-awake
    src/main.cpp
    src/app.cpp
    src/cli_parser.cpp
)

target_include_directories(keep-awake PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

# 按平台选择具体实现（platform 抽象 + tray 抽象）
if(WIN32)
    target_sources(keep-awake PRIVATE src/platform_win.cpp src/tray_win.cpp)
    target_link_libraries(keep-awake PRIVATE user32 shell32)

    # 嵌入托盘图标资源（assets/tray.ico）
    set(KEEP_AWAKE_RESOURCE_H "${CMAKE_CURRENT_SOURCE_DIR}/include/resource.h")
    set(KEEP_AWAKE_ICON "${CMAKE_CURRENT_SOURCE_DIR}/assets/tray.ico")
    configure_file(resources/app.rc.in ${CMAKE_CURRENT_BINARY_DIR}/app.rc @ONLY)
    target_sources(keep-awake PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/app.rc)
elseif(APPLE)
    enable_language(OBJCXX)
    target_sources(keep-awake PRIVATE src/platform_mac.mm src/tray_mac.mm)
    target_link_libraries(keep-awake PRIVATE "-framework IOKit" "-framework AppKit" "-framework CoreFoundation")
else()
    target_sources(keep-awake PRIVATE src/platform_linux.cpp src/tray_linux.cpp)
endif()

# MSVC：/utf-8 保证中文注释源码以 UTF-8 正确解析
if(MSVC)
    target_compile_options(keep-awake PRIVATE /W4 /utf-8)
endif()
