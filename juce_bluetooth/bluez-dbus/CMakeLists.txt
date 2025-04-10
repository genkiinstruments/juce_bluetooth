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

foreach(interface ${BLUEZ_INTERFACES})
    set(output_base "${CMAKE_CURRENT_BINARY_DIR}/org-bluez-${interface}")
    set(input_xml "${CMAKE_CURRENT_SOURCE_DIR}/dbus-bluez-v5.48/org.bluez.${interface}.xml")

    # Execute gdbus-codegen immediately
    execute_process(
        COMMAND gdbus-codegen
                --interface-prefix "org.bluez.${interface}."
                --generate-c-code "${output_base}"
                "${input_xml}"
        RESULT_VARIABLE result
        ERROR_VARIABLE error_output
    )

    # Check for errors
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "Failed to generate D-Bus interface for ${interface}: ${error_output}")
    endif()

    list(APPEND GENERATED_SOURCES "${output_base}.c")
    list(APPEND GENERATED_HEADERS "${output_base}.h")
endforeach()

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
