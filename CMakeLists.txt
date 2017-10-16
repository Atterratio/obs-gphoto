cmake_minimum_required(VERSION 2.8)
project(obs-gphoto)
set(obs-gphoto_DESCRIPTION "Allows capture DSLR cameras preview through gPhoto in obs-studio on Linux")
set(obs-gphoto_VERSION_MAJOR 0)
set(obs-gphoto_VERSION_MINOR 1)
set(obs-gphoto_VERSION_PATCH 1)
set(CMAKE_BUILD_TYPE Release)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/modules/")

find_package(LibObs)
if(${LibObs_FOUND})
    message(STATUS "LibObs FOUND")
else(${LibObs_FOUND})
    message(FATAL_ERROR "LibObs NOT FOUND")
endif(${LibObs_FOUND})

find_package(Gphoto2)
if(${GPHOTO2_FOUND})
    message(STATUS "Gphoto2 FOUND")
else(${GPHOTO2_FOUND})
    message(FATAL_ERROR "Gphoto2 NOT FOUND")
endif(${GPHOTO2_FOUND})

find_package(ImageMagick COMPONENTS MagickCore)

find_package(udev)
if(NOT UDEV_FOUND OR DISABLE_UDEV)
    message(STATUS "udev disabled for v4l2 plugin")
else()
    set(gphoto-udev_SOURCES src/gphoto-udev.c src/gphoto-udev.h)
    add_definitions(-DHAVE_UDEV)
endif()

math(EXPR BITS "8*${CMAKE_SIZEOF_VOID_P}")
set(PLUGIN_DIRECTORY "${CMAKE_BINARY_DIR}/build/obs-gphoto")
set(PLUGIN_BIN_DIRECTORY "${PLUGIN_DIRECTORY}/bin/${BITS}bit")

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PLUGIN_BIN_DIRECTORY})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PLUGIN_BIN_DIRECTORY})

include_directories(src ${LIBOBS_INCLUDE_DIRS} ${Gphoto2_INCLUDE_DIRS} ${ImageMagick_MagickCore_INCLUDE_DIRS} ${UDEV_INCLUDE_DIR})

set(SOURCE_FILES src/obs-gphoto.c src/gphoto-utils.c src/gphoto-utils.h src/live-preview.c ${gphoto-udev_SOURCES} src/live-preview.h)
add_library(obs-gphoto MODULE ${SOURCE_FILES})

SET_TARGET_PROPERTIES(obs-gphoto PROPERTIES PREFIX "")
target_link_libraries(obs-gphoto ${LIBOBS_LIBRARIES} ${Gphoto2_LIBRARIES} ${ImageMagick_LIBRARIES} ${UDEV_LIBRARIES})

# install
if(${SYSTEM_INSTALL})
    install(TARGETS obs-gphoto DESTINATION /usr/lib/obs-plugins)
else()
    install(DIRECTORY ${PLUGIN_DIRECTORY} DESTINATION $ENV{HOME}/.config/obs-studio/plugins USE_SOURCE_PERMISSIONS)
endif()