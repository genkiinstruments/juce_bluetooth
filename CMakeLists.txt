cmake_minimum_required(VERSION 3.15)

project(juce_bluetooth VERSION 1.1.0)

if (${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_CURRENT_SOURCE_DIR})
  include(FetchContent)
  FetchContent_Declare(JUCE
          GIT_REPOSITORY https://github.com/juce-framework/JUCE
          GIT_TAG 7.0.7
  )
  FetchContent_Declare(Microsoft.GSL
          GIT_REPOSITORY https://github.com/microsoft/GSL
          GIT_TAG "v4.0.0"
          GIT_SHALLOW ON
  )
  FetchContent_Declare(fmt
          GIT_REPOSITORY https://github.com/fmtlib/fmt
          GIT_TAG 7.1.3
  )
  FetchContent_MakeAvailable(JUCE Microsoft.GSL fmt)
endif ()


include(GNUInstallDirs)
juce_add_module(${PROJECT_NAME} INSTALL_PATH ${CMAKE_INSTALL_INCLUDEDIR})
target_include_directories(${PROJECT_NAME} INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/juce_bluetooth)

add_library(genki::bluetooth ALIAS ${PROJECT_NAME})
target_link_libraries(${PROJECT_NAME} INTERFACE GSL fmt range-v3)

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
