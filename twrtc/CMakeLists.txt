cmake_minimum_required(VERSION 3.16)
project(twrtc LANGUAGES CXX)

if(QT_DEPENDENCY)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core)
find_package(Qt${QT_VERSION_MAJOR}
    COMPONENTS
        Core
        Gui
        Widgets
)
set(PROJECT_SOURCES
    main.cpp
    # twrtc.ui
    # twrtc.h
    twrtc.cpp
)
else()
set(PROJECT_SOURCES
    main.cpp
)
endif()



add_executable(${PROJECT_NAME} ${PROJECT_SOURCES})

target_compile_definitions(${PROJECT_NAME}
    PRIVATE
        WEBRTC_WIN
        QT_NO_KEYWORDS
        NOMINMAX
)

# set_target_properties(${PROJECT_NAME}
#     PROPERTIES
#         WIN32_EXECUTABLE TRUE
# )

target_include_directories(${PROJECT_NAME}
    PRIVATE
        ${CMAKE_SOURCE_DIR}/3rd/include
        ${CMAKE_SOURCE_DIR}/3rd/include/rtc
)

target_link_directories(${PROJECT_NAME}
    PRIVATE
        ${CMAKE_SOURCE_DIR}/3rd/lib
)
if(QT_DEPENDENCY)
target_compile_definitions(${PROJECT_NAME}
    PRIVATE
        qt_dependency
)
target_link_libraries(${PROJECT_NAME}
    PUBLIC
        Qt::Core
        Qt::Gui
        Qt::Widgets
)
endif()
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
        absl::string_view
    )
else()
    message("Build Type: ${CMAKE_BUILD_TYPE}")
    target_link_libraries(${PROJECT_NAME}
        PRIVATE
            webrtc
            absl::string_view
    )
endif()
