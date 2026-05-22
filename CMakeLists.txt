cmake_minimum_required(VERSION 3.16)
project(obs-kinect VERSION 1.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# ── Find dependencies ────────────────────────────────────────────────────────

find_package(libobs REQUIRED)
find_package(PkgConfig REQUIRED)

pkg_check_modules(FREENECT REQUIRED libfreenect)
pkg_check_modules(LIBUSB REQUIRED libusb-1.0)

# ── Shared include dirs ──────────────────────────────────────────────────────

set(OBS_KINECT_INCLUDE_DIRS
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${libobs_INCLUDE_DIRS}
    /usr/include/obs
)

# ── obs-kinectcore ───────────────────────────────────────────────────────────
# The shared core library that all backends depend on

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
# The main OBS plugin (registers the Kinect source in OBS)

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
)

# ── obs-kinect-freenect ──────────────────────────────────────────────────────
# The Kinect v1 backend using libfreenect

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
)

# ── Install ──────────────────────────────────────────────────────────────────

include(GNUInstallDirs)

# Plugin dir: ~/.config/obs-studio/plugins/obs-kinect/
set(OBS_PLUGIN_DESTINATION "$ENV{HOME}/.config/obs-studio/plugins/obs-kinect")
set(OBS_PLUGIN_BIN "${OBS_PLUGIN_DESTINATION}/bin/64bit")

# Set RPATH so plugins find obs-kinectcore.so in the same directory at runtime
set_target_properties(obs-kinect PROPERTIES
    INSTALL_RPATH "${OBS_PLUGIN_BIN}"
    BUILD_WITH_INSTALL_RPATH TRUE
)

set_target_properties(obs-kinect-freenect PROPERTIES
    INSTALL_RPATH "${OBS_PLUGIN_BIN}"
    BUILD_WITH_INSTALL_RPATH TRUE
)

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
message(STATUS "  libobs include : ${libobs_INCLUDE_DIRS}")
message(STATUS "  libfreenect    : ${FREENECT_LIBRARIES}")
message(STATUS "  libusb         : ${LIBUSB_LIBRARIES}")
message(STATUS "  Install to     : ${OBS_PLUGIN_DESTINATION}")
message(STATUS "")
