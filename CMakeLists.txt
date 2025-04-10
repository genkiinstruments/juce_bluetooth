cmake_minimum_required(VERSION 3.15)

project(juce_bluetooth VERSION 1.1.0)
set(CMAKE_CXX_STANDARD 20)

include(cmake/CPM.cmake)

# NOTE: CPM calculates a hash based on the arguments passed to CPMAddPackage(...)
#       We need to pass in absolute paths to the the patches to avoid downloading multiple copies of JUCE...
get_filename_component(cpm_abs "${CPM_SOURCE_CACHE}" ABSOLUTE)
file(GLOB patch_files "${CMAKE_CURRENT_SOURCE_DIR}/cmake/*.patch")
file(COPY ${patch_files} DESTINATION ${cpm_abs}/patches)

if (${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_CURRENT_SOURCE_DIR})
    CPMAddPackage(NAME JUCE
        GITHUB_REPOSITORY juce-framework/JUCE
        GIT_TAG 8.0.3
        PATCH_COMMAND git apply --ignore-space-change ${cpm_abs}/patches/juce_Messaging_linux.cpp.patch
        UPDATE_DISCONNECTED 1
        OPTIONS "JUCE_BUILD_EXTRAS OFF" "JUCE_BUILD_EXAMPLES OFF" "JUCE_ENABLE_MODULE_SOURCE_GROUPS ON"
)
endif ()

CPMAddPackage("gh:microsoft/GSL#v4.0.0")
CPMAddPackage("gh:fmtlib/fmt#10.1.1")
CPMAddPackage("gh:ericniebler/range-v3#0.12.0")

include(GNUInstallDirs)

juce_add_module(${PROJECT_NAME} INSTALL_PATH ${CMAKE_INSTALL_INCLUDEDIR})
target_include_directories(${PROJECT_NAME} INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/juce_bluetooth)
target_compile_definitions(${PROJECT_NAME} INTERFACE JUCE_LINUX_USE_GLIB_MAINLOOP=1)

add_library(genki::bluetooth ALIAS ${PROJECT_NAME})
target_link_libraries(${PROJECT_NAME} INTERFACE GSL fmt range-v3)

if (UNIX AND NOT APPLE)
    find_package(PkgConfig REQUIRED)

    pkg_check_modules(GLIB REQUIRED glib-2.0)
    pkg_search_module(GIO_UNIX REQUIRED gio-unix-2.0)

    target_include_directories(${PROJECT_NAME} INTERFACE ${GLIB_INCLUDE_DIRS})
    target_link_directories(${PROJECT_NAME} INTERFACE ${GLIB_LIBRARY_DIRS})
    target_link_libraries(${PROJECT_NAME} INTERFACE ${GLIB_LIBRARIES})

    target_include_directories(${PROJECT_NAME} INTERFACE ${GIO_UNIX_INCLUDE_DIRS})
    target_link_directories(${PROJECT_NAME} INTERFACE ${GIO_UNIX_LIBRARY_DIRS})
    target_link_libraries(${PROJECT_NAME} INTERFACE ${GIO_UNIX_LIBRARIES})

    add_subdirectory(juce_bluetooth/bluez-dbus)
    target_link_libraries(${PROJECT_NAME} INTERFACE bluez-dbus)

    target_compile_options(${PROJECT_NAME} INTERFACE $<$<COMPILE_LANGUAGE:C>:-w>)
endif ()

option(JUCE_BLUETOOTH_BUILD_EXAMPLES OFF "Enable juce_bluetooth example builds")

if (JUCE_BLUETOOTH_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif ()

if (${CMAKE_PROJECT_NAME} STREQUAL ${PROJECT_NAME})
    include(CMakePackageConfigHelpers)
    write_basic_package_version_file("${PROJECT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake"
            VERSION ${PROJECT_VERSION}
            COMPATIBILITY AnyNewerVersion
            )
    configure_package_config_file("${CMAKE_CURRENT_SOURCE_DIR}/cmake/Config.cmake.in"
            "${PROJECT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
            INSTALL_DESTINATION lib/cmake/${PROJECT_NAME}
            )

    install(FILES "${PROJECT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
            "${PROJECT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake"
            DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/${PROJECT_NAME}/cmake)

    set(CMAKE_SUPPRESS_REGENERATION ON)
endif ()
