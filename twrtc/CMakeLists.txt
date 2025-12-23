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
        Network
        WebSockets
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


file(GLOB_RECURSE module ${CMAKE_CURRENT_SOURCE_DIR}/module/*.cpp)
message("Module Source Files: ${module}")
add_executable(${PROJECT_NAME} ${PROJECT_SOURCES} ${module})

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
        ${CMAKE_SOURCE_DIR}/3rd/include/rtc/3rd
        ${CMAKE_SOURCE_DIR}/3rd/include/libyuv
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
        Qt::Network
        Qt::WebSockets
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
            dmoguids
            amstrmid
            wmcodecdspuuid
            msdmo
            strmiids  # IID_IMediaObject / IID_IMediaBuffer (DirectShow/DMO interface IIDs)
            iphlpapi
    )
endif()

if("Debug" STREQUAL "${CMAKE_BUILD_TYPE}")
    message("Build Type: ${CMAKE_BUILD_TYPE}")
    target_link_libraries(${PROJECT_NAME}
        PRIVATE
        webrtcd
        # JsonCpp is required by rtc_base/strings/json.h helpers and conductor.cpp
        jsoncppd
        absl::string_view
        absl::flags
    )
else()
    message("Build Type: ${CMAKE_BUILD_TYPE}")
    target_link_libraries(${PROJECT_NAME}
        PRIVATE
            webrtc
            # JsonCpp is required by rtc_base/strings/json.h helpers and conductor.cpp
            jsoncpp
            absl::string_view
            absl::flags
    )
endif()
