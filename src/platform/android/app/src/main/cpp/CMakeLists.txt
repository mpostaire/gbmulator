# For more information about using CMake with Android Studio, read the
# documentation: https://d.android.com/studio/projects/add-native-code.html

cmake_minimum_required(VERSION 3.22.1)

project("gbmulator")

find_package (oboe REQUIRED CONFIG)

include(FetchContent)

FetchContent_Declare(
    openal-soft
    GIT_REPOSITORY https://github.com/kcat/openal-soft.git
    GIT_TAG latest
)

set(ALSOFT_EXAMPLES OFF CACHE BOOL "" FORCE)
set(ALSOFT_TESTS OFF CACHE BOOL "" FORCE)
set(ALSOFT_BACKEND_PIPEWIRE OFF CACHE BOOL "" FORCE)
set(ALSOFT_BACKEND_OBOE ON CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(openal-soft)

# Creates your game shared library. The name must be the same as the
# one used for loading in your Kotlin/Java or AndroidManifest.txt files.
add_library(gbmulator SHARED
    main.cpp
    AndroidOut.cpp
)

# Searches for a package provided by the game activity dependency
find_package(game-activity REQUIRED CONFIG)

# Forces the linker to keep the JNI entry point for GameActivity
set(CMAKE_SHARED_LINKER_FLAGS
    "${CMAKE_SHARED_LINKER_FLAGS} -u Java_com_google_androidgamesdk_GameActivity_initializeNativeCode"
)

# NDK sysroot
set(NDK_SYSROOT ${ANDROID_NDK}/toolchains/llvm/prebuilt/${CMAKE_HOST_SYSTEM_NAME}-x86_64/sysroot)
set(ZLIB_INCLUDE ${NDK_SYSROOT}/usr/include)
set(GLES_INCLUDE ${NDK_SYSROOT}/usr/include/GLES3)
set(EGL_INCLUDE  ${NDK_SYSROOT}/usr/include/EGL)
set(GBMULATOR_ROOT_DIR ${CMAKE_SOURCE_DIR}/../../../../../../..)
set(GBMULATOR_BUILD_DIR ${GBMULATOR_ROOT_DIR}/build/android/${CMAKE_ANDROID_ARCH})
add_custom_command(
    OUTPUT ${GBMULATOR_BUILD_DIR}/gbmulator.a
    WORKING_DIRECTORY ${GBMULATOR_ROOT_DIR}
    COMMAND make PLATFORM=android CC=${CMAKE_C_COMPILER} BOOTROM_ODIR=${GBMULATOR_ROOT_DIR}/build/bootroms ROOT_ODIR=${GBMULATOR_BUILD_DIR} "EXTRA_CFLAGS=-fPIC --target=${CMAKE_C_COMPILER_TARGET} -I${openal-soft_SOURCE_DIR}/include -I${GLES_INCLUDE} -I${EGL_INCLUDE} -I${ZLIB_INCLUDE}"
)
add_custom_target(gbmulator_lib DEPENDS ${GBMULATOR_BUILD_DIR}/gbmulator.a)
add_dependencies(gbmulator gbmulator_lib)

# Configure libraries CMake uses to link your target library.
target_link_libraries(gbmulator
    oboe::oboe
    OpenAL::OpenAL
    ${GBMULATOR_BUILD_DIR}/gbmulator.a
    # The game activity
    game-activity::game-activity_static
    # EGL and other dependent libraries required for drawing
    # and interacting with Android system
    EGL
    GLESv3
    android
    log
    z
)
