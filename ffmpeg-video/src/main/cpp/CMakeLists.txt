# For more information about using CMake with Android Studio, read the
# documentation: https://d.android.com/studio/projects/add-native-code.html

# Sets the minimum version of CMake required to build the native library.

cmake_minimum_required(VERSION 3.4.1)

# Creates and names a library, sets it as either STATIC
# or SHARED, and provides the relative paths to its source code.
# You can define multiple libraries, and CMake builds them for you.
# Gradle automatically packages shared libraries with your APK.
set(FFMPEG_DIR ${CMAKE_SOURCE_DIR}/../../../../ffmpeg/)
file(GLOB FFMPEG_HEADER ${FFMPEG_DIR}/include/*)

link_directories(${FFMPEG_DIR}/libs/)
include_directories(${FFMPEG_DIR}/include/)

add_library(video-lib SHARED video-lib.cpp VideoPlayer.h VideoPlayer.cpp)
add_library(video-test-lib SHARED video-test-lib.cpp)

add_library(avcodec SHARED IMPORTED)
set_target_properties(avcodec PROPERTIES IMPORTED_LOCATION "${FFMPEG_DIR}/libs/libavcodec.so")
add_library(avformat SHARED IMPORTED)
set_target_properties(avformat PROPERTIES IMPORTED_LOCATION "${FFMPEG_DIR}/libs/libavformat.so")
add_library(avutil SHARED IMPORTED)
set_target_properties(avutil PROPERTIES IMPORTED_LOCATION "${FFMPEG_DIR}/libs/libavutil.so")
add_library(swresample SHARED IMPORTED)
set_target_properties(swresample PROPERTIES IMPORTED_LOCATION "${FFMPEG_DIR}/libs/libswresample.so")
add_library(swscale SHARED IMPORTED)
set_target_properties(swscale PROPERTIES IMPORTED_LOCATION "${FFMPEG_DIR}/libs/libswscale.so")
add_library(avfilter SHARED IMPORTED)
set_target_properties(avfilter PROPERTIES IMPORTED_LOCATION "${FFMPEG_DIR}/libs/libavfilter.so")
add_library(avdevice SHARED IMPORTED)
set_target_properties(avdevice PROPERTIES IMPORTED_LOCATION "${FFMPEG_DIR}/libs/libavdevice.so")

# Searches for a specified prebuilt library and stores the path as a
# variable. Because CMake includes system libraries in the search path by
# default, you only need to specify the name of the public NDK library
# you want to add. CMake verifies that the library exists before
# completing its build.

find_library(log-lib log)
# Specifies libraries CMake should link to your target library. You
# can link multiple libraries, such as libraries you define in this
# build script, prebuilt third-party libraries, or system libraries.

target_link_libraries(video-lib avutil swresample avcodec avfilter avdevice swscale avformat android OpenSLES ${log-lib} )
target_link_libraries(video-test-lib avutil swresample avcodec avfilter avdevice swscale avformat -ljnigraphics OpenSLES ${log-lib} )