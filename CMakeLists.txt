cmake_minimum_required(VERSION 3.16)

project("twrtc")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 3rd/lib 里的 webrtcd.lib 采用静态运行库（MTd）编译。
# 为避免 LNK2038 RuntimeLibrary 不匹配，这里强制本工程也使用 /MTd（Debug）与 /MT（Release）。
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

set(ABSL_USE_GOOGLETEST_HEAD ON)

# 对应于 -DABSL_BUILD_TESTING=OFF (使用最后指定的值)
set(ABSL_BUILD_TESTING OFF)

set(QT_DEPENDENCY ON)

option(TWRTC_USE_INSTALLED_ABSL "Use installed Abseil (find_package) instead of building 3rd/abseil-cpp" ON)

if(TWRTC_USE_INSTALLED_ABSL)
	# Prefer a prebuilt/installed Abseil to avoid rebuilding it every time.
	# Provide it via CMAKE_PREFIX_PATH or absl_DIR (folder containing abslConfig.cmake).
	find_package(absl CONFIG QUIET)
endif()

if(NOT absl_FOUND)
	add_subdirectory("3rd/abseil-cpp")
endif()
add_subdirectory("twrtc")