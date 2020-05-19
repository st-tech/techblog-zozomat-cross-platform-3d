# For more information about using CMake with Android Studio, read the
# documentation: https://d.android.com/studio/projects/add-native-code.html

# Sets the minimum version of CMake required to build the native library.

cmake_minimum_required(VERSION 3.4.1)

set(common_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../../common)

message( ${CMAKE_CURRENT_SOURCE_DIR} )
message(AUTHOR_WARNING "com.zozo.ztr_android running CMake...")

add_library( # Sets the name of the library.
        native-lib-renderer

        # Sets the library as a shared library.
        SHARED

        # Provides a relative path to your source file(s).
        RenderLib_ndk.cpp

        ${common_DIR}/ztr_platform_independent_layer.cpp)

# Searches for a specified prebuilt library and stores the path as a
# variable. Because CMake includes system libraries in the search path by
# default, you only need to specify the name of the public NDK library
# you want to add. CMake verifies that the library exists before
# completing its build.

find_library( # Sets the name of the path variable.
        log-lib log
        android-lib

        # Specifies the name of the NDK library that
        # you want CMake to locate.

        android)

# Specifies libraries CMake should link to your target library. You
# can link multiple libraries, such as libraries you define in this
# build script, prebuilt third-party libraries, or system libraries.

target_link_libraries( # Specifies the target library.
        native-lib-renderer
        GLESv3
        EGL
        android

        # Links the target library to the log library
        # included in the NDK.
        ${log-lib})

target_include_directories(native-lib-renderer PRIVATE
        ${common_DIR} ${foot_model_bin_DIR})
