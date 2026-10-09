# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) 2026 MihiOr. See the repository LICENSE.txt.
# Include AFTER add_subdirectory(cmake/stm32cubemx) in top-level CMakeLists.
# Reapply source selection on every configure, including after regeneration.
if(NOT TARGET ${CMAKE_PROJECT_NAME} OR NOT TARGET stm32cubemx)
    message(FATAL_ERROR "Include NativeUsbSerial.cmake after CubeMX's add_subdirectory")
endif()
get_target_property(_native_usb_sources ${CMAKE_PROJECT_NAME} SOURCES)
set(_native_usb_kept "")
set(_native_usb_replaced 0)
foreach(_native_usb_src IN LISTS _native_usb_sources)
    get_filename_component(_native_usb_name "${_native_usb_src}" NAME)
    if(_native_usb_name MATCHES "^(usb_device|usbd_conf|usbd_cdc_if)\\.c$")
        math(EXPR _native_usb_replaced "${_native_usb_replaced} + 1")
    elseif(_native_usb_name MATCHES "^(sysmem|syscalls)\\.c$")
        # Some CubeMX releases list these sources without creating them in
        # a fresh CMake project. Preserve generated/custom runtime files when
        # present, otherwise select the bundled, unmodified ST templates.
        get_filename_component(_native_usb_absolute "${_native_usb_src}"
            ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
        if(EXISTS "${_native_usb_absolute}")
            list(APPEND _native_usb_kept "${_native_usb_src}")
        else()
            set(_native_usb_fallback "${CMAKE_CURRENT_LIST_DIR}/runtime/${_native_usb_name}")
            if(NOT EXISTS "${_native_usb_fallback}")
                message(FATAL_ERROR "Missing ${_native_usb_name}: copy the entire NativeUsbSerial folder, including runtime/")
            endif()
            message(STATUS "NativeUsbSerial: CubeMX omitted ${_native_usb_name}; using bundled ST runtime template")
            list(APPEND _native_usb_kept "${_native_usb_fallback}")
        endif()
    else()
        list(APPEND _native_usb_kept "${_native_usb_src}")
    endif()
endforeach()
if(NOT _native_usb_replaced EQUAL 3)
    message(FATAL_ERROR "NativeUsbSerial expects CubeMX USB_DEVICE CDC (3 generated USB application sources)")
endif()
set_property(TARGET ${CMAKE_PROJECT_NAME} PROPERTY SOURCES "${_native_usb_kept}")
target_sources(${CMAKE_PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_LIST_DIR}/native_usb_serial.c")
target_include_directories(stm32cubemx INTERFACE "${CMAKE_CURRENT_LIST_DIR}")
target_link_options(${CMAKE_PROJECT_NAME} PRIVATE
    "LINKER:--wrap=HAL_PCD_IRQHandler"
    "LINKER:--wrap=USBD_LL_DataInStage"
    -u _printf_float
)
unset(_native_usb_sources)
unset(_native_usb_kept)
unset(_native_usb_replaced)
unset(_native_usb_src)
unset(_native_usb_name)
unset(_native_usb_absolute)
unset(_native_usb_fallback)
