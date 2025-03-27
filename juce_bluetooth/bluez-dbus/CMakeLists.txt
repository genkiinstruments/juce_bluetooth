cmake_minimum_required(VERSION 3.17)

project(bluez-dbus VERSION 5.48)

# Define the interfaces you want to generate
set(BLUEZ_INTERFACES
    "Adapter1"
    "Device1"
    "GattService1"
    "GattCharacteristic1"
    "GattDescriptor1"
    "Battery1"
)

# List to collect all generated sources
set(GENERATED_SOURCES "")
set(GENERATED_HEADERS "")

# Generate D-Bus interfaces from XML
foreach(interface ${BLUEZ_INTERFACES})
    set(output_base "${CMAKE_CURRENT_BINARY_DIR}/org-bluez-${interface}")
    set(input_xml "${CMAKE_CURRENT_SOURCE_DIR}/dbus-bluez-v5.48/org.bluez.${interface}.xml")

    add_custom_command(
        OUTPUT "${output_base}.c" "${output_base}.h"
        COMMAND gdbus-codegen
                --interface-prefix "org.bluez.${interface}."
                --generate-c-code "${output_base}"
                "${input_xml}"
        DEPENDS "${input_xml}"
        COMMENT "Generating D-Bus interface for org.bluez.${interface}"
    )

    list(APPEND GENERATED_SOURCES "${output_base}.c" "${output_base}.h")
endforeach()

# Make sure the sources actually get generated
add_custom_target(bluez_dbus_generated_sources DEPENDS ${GENERATED_SOURCES})

# Disable warnings for generated files
foreach(source ${GENERATED_SOURCES})
    set_source_files_properties(
        ${source}
        PROPERTIES
        COMPILE_FLAGS "-w"
    )
endforeach()

add_library(${PROJECT_NAME} INTERFACE)
target_sources(${PROJECT_NAME} INTERFACE ${GENERATED_SOURCES})
target_include_directories(${PROJECT_NAME} INTERFACE ${CMAKE_CURRENT_BINARY_DIR})
