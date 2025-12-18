cmake_minimum_required(VERSION 3.16)
project(twrtc LANGUAGES CXX)

include(qt.cmake)

#set(CMAKE_PREFIX_PATH "D:/developTool/absl-install")

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core)
find_package(Qt${QT_VERSION_MAJOR}
    COMPONENTS
        Core
        Gui
        Widgets
)
qt_standard_project_setup()

set(PROJECT_SOURCES
    main.cpp
    twrtc.ui
    twrtc.h
    twrtc.cpp
)

qt_add_executable(${PROJECT_NAME} ${PROJECT_SOURCES})

target_compile_definitions(${PROJECT_NAME}
    PRIVATE
        WEBRTC_WIN
        QT_NO_KEYWORDS
        NOMINMAX
)

set_target_properties(${PROJECT_NAME}
    PROPERTIES
        WIN32_EXECUTABLE TRUE
)

target_include_directories(${PROJECT_NAME}
    PRIVATE
        ${CMAKE_SOURCE_DIR}/3rd/include
        ${CMAKE_SOURCE_DIR}/3rd/include/rtc
)

target_link_directories(${PROJECT_NAME}
    PRIVATE
        ${CMAKE_SOURCE_DIR}/3rd/lib
)

target_link_libraries(${PROJECT_NAME}
    PUBLIC
        Qt::Core
        Qt::Gui
        Qt::Widgets
        absl::string_view
)

# webrtc/rtc 在 Windows 下会用到 WinMM 的 timeGetTime 等API
if(WIN32)
    target_link_libraries(${PROJECT_NAME}
        PRIVATE
            winmm
            d3d11     # D3D11CreateDevice
            dxgi      # CreateDXGIFactory1
            dwmapi    # DwmGetWindowAttribute
            shcore    # GetDpiForMonitor
    )
endif()

if("Debug" STREQUAL "${CMAKE_BUILD_TYPE}")
    message("Build Type: ${CMAKE_BUILD_TYPE}")
    target_link_libraries(${PROJECT_NAME}
        PRIVATE
        webrtcd
    )
else()
    message("Build Type: ${CMAKE_BUILD_TYPE}")
    target_link_libraries(${PROJECT_NAME}
        PRIVATE
            webrtc
    )
endif()
