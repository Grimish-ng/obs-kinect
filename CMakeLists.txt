cmake_minimum_required(VERSION 3.16)
project(obs-kinect VERSION 1.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# ── Find libobs ──────────────────────────────────────────────────────────────
# Search common cmake config locations across distros:
#   Fedora/RHEL:  /usr/lib64/cmake/libobs
#   Arch:         /usr/lib/cmake/libobs
#   Ubuntu/Debian:/usr/lib/cmake/libobs or /usr/lib/x86_64-linux-gnu/cmake/libobs

find_package(libobs QUIET)

if(NOT libobs_FOUND)
    # Try common paths explicitly
    foreach(SEARCH_PATH
        "/usr/lib64/cmake/libobs"
        "/usr/lib/cmake/libobs"
        "/usr/lib/x86_64-linux-gnu/cmake/libobs"
        "/usr/local/lib/cmake/libobs"
        "/usr/local/lib64/cmake/libobs"
    )
        if(EXISTS "${SEARCH_PATH}/libobsConfig.cmake")
            set(libobs_DIR "${SEARCH_PATH}")
            find_package(libobs REQUIRED)
            break()
        endif()
    endforeach()
endif()

if(NOT libobs_FOUND)
    message(FATAL_ERROR "Could not find libobs. Install obs-studio-devel (Fedora), obs-studio (Arch), or libobs-dev (Ubuntu) and try again.\nOr pass -Dlibobs_DIR=/path/to/cmake/libobs")
endif()

message(STATUS "Found libobs at: ${libobs_DIR}")

# ── Find other dependencies ──────────────────────────────────────────────────

find_package(PkgConfig REQUIRED)
pkg_check_modules(FREENECT REQUIRED libfreenect)
pkg_check_modules(LIBUSB REQUIRED libusb-1.0)

# ── Find OBS headers ─────────────────────────────────────────────────────────
# obs-module.h location varies by distro

find_path(OBS_INCLUDE_DIR
    NAMES obs-module.h
    PATHS
        /usr/include/obs
        /usr/local/include/obs
    REQUIRED
)
message(STATUS "Found OBS headers at: ${OBS_INCLUDE_DIR}")

# ── Shared include dirs ──────────────────────────────────────────────────────

set(OBS_KINECT_INCLUDE_DIRS
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${libobs_INCLUDE_DIRS}
    ${OBS_INCLUDE_DIR}
)

# ── Install destination ──────────────────────────────────────────────────────

set(OBS_PLUGIN_DESTINATION "$ENV{HOME}/.config/obs-studio/plugins/obs-kinect")
set(OBS_PLUGIN_BIN "${OBS_PLUGIN_DESTINATION}/bin/64bit")

# ── obs-kinectcore ───────────────────────────────────────────────────────────

add_library(obs-kinectcore SHARED
    src/obs-kinect-core/Enums.cpp
    src/obs-kinect-core/Helper.cpp
    src/obs-kinect-core/KinectDevice.cpp
    src/obs-kinect-core/KinectDeviceAccess.cpp
    src/obs-kinect-core/KinectPluginImpl.cpp
)

target_include_directories(obs-kinectcore PRIVATE ${OBS_KINECT_INCLUDE_DIRS})
target_compile_definitions(obs-kinectcore PRIVATE OBS_KINECT_CORE_EXPORT)
target_link_libraries(obs-kinectcore PRIVATE obs)
target_compile_options(obs-kinectcore PRIVATE -Wall -Wextra)

set_target_properties(obs-kinectcore PROPERTIES
    PREFIX ""
    SUFFIX ".so"
)

# ── obs-kinect ───────────────────────────────────────────────────────────────

add_library(obs-kinect SHARED
    src/obs-kinect/GreenscreenEffects/BlurBackgroundEffect.cpp
    src/obs-kinect/GreenscreenEffects/RemoveBackgroundEffect.cpp
    src/obs-kinect/GreenscreenEffects/ReplaceBackgroundEffect.cpp
    src/obs-kinect/Shaders/AlphaMaskShader.cpp
    src/obs-kinect/Shaders/ConvertDepthIRToColorShader.cpp
    src/obs-kinect/Shaders/GaussianBlurShader.cpp
    src/obs-kinect/Shaders/GreenScreenFilterShader.cpp
    src/obs-kinect/Shaders/TextureLerpShader.cpp
    src/obs-kinect/Shaders/VisibilityMaskShader.cpp
    src/obs-kinect/KinectDeviceRegistry.cpp
    src/obs-kinect/KinectPlugin.cpp
    src/obs-kinect/KinectSource.cpp
    src/obs-kinect/kinect-plugin.cpp
)

target_include_directories(obs-kinect PRIVATE ${OBS_KINECT_INCLUDE_DIRS})
target_link_libraries(obs-kinect PRIVATE obs-kinectcore obs pthread)
target_compile_options(obs-kinect PRIVATE -Wall -Wextra)

set_target_properties(obs-kinect PROPERTIES
    PREFIX ""
    SUFFIX ".so"
    INSTALL_RPATH "${OBS_PLUGIN_BIN}"
    BUILD_WITH_INSTALL_RPATH TRUE
)

# ── obs-kinect-freenect ──────────────────────────────────────────────────────

add_library(obs-kinect-freenect SHARED
    src/obs-kinect-freenect/Export.cpp
    src/obs-kinect-freenect/FreenectDevice.cpp
    src/obs-kinect-freenect/FreenectPlugin.cpp
)

target_include_directories(obs-kinect-freenect PRIVATE
    ${OBS_KINECT_INCLUDE_DIRS}
    ${FREENECT_INCLUDE_DIRS}
    ${LIBUSB_INCLUDE_DIRS}
)

target_link_libraries(obs-kinect-freenect PRIVATE
    obs-kinectcore
    obs
    ${FREENECT_LIBRARIES}
    ${LIBUSB_LIBRARIES}
    pthread
)

target_compile_options(obs-kinect-freenect PRIVATE -Wall -Wextra)

set_target_properties(obs-kinect-freenect PROPERTIES
    PREFIX ""
    SUFFIX ".so"
    INSTALL_RPATH "${OBS_PLUGIN_BIN}"
    BUILD_WITH_INSTALL_RPATH TRUE
)

# ── Install ──────────────────────────────────────────────────────────────────

install(TARGETS obs-kinect obs-kinect-freenect
    LIBRARY DESTINATION "${OBS_PLUGIN_BIN}"
)

install(TARGETS obs-kinectcore
    LIBRARY DESTINATION "${OBS_PLUGIN_BIN}"
)

install(DIRECTORY data/obs-plugins/obs-kinect/
    DESTINATION "${OBS_PLUGIN_DESTINATION}/data"
    OPTIONAL
)

install(DIRECTORY data/obs-plugins/obs-kinect/locale/
    DESTINATION "${OBS_PLUGIN_DESTINATION}/data/locale"
    OPTIONAL
)

# ── Summary ──────────────────────────────────────────────────────────────────

message(STATUS "")
message(STATUS "obs-kinect Linux build (libfreenect v1 backend)")
message(STATUS "  libobs         : ${libobs_DIR}")
message(STATUS "  OBS headers    : ${OBS_INCLUDE_DIR}")
message(STATUS "  libfreenect    : ${FREENECT_LIBRARIES}")
message(STATUS "  libusb         : ${LIBUSB_LIBRARIES}")
message(STATUS "  Install to     : ${OBS_PLUGIN_DESTINATION}")
message(STATUS "")
message(STATUS "Build with:")
message(STATUS "  cmake -B build && cmake --build build -j\$(nproc) && cmake --install build")
message(STATUS "")
