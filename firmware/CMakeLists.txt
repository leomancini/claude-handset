cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)   # for clangd / editors

# Board selection. claude_handset is the real PCB; claude_handset_kb2040 is the
# bench build for an Adafruit KB2040 wired to the same GPIO numbers.
if (NOT PICO_BOARD)
    set(PICO_BOARD claude_handset)
endif()
list(APPEND PICO_BOARD_HEADER_DIRS ${CMAKE_CURRENT_LIST_DIR}/boards)

include(pico_sdk_import.cmake)

project(claude_handset_firmware C CXX ASM)

pico_sdk_init()

add_executable(claude_handset
    src/main.c
    src/usb_descriptors.c
    src/usb_audio.c
    src/usb_reset.c
    src/i2s_out.c
    src/pdm_mic.c
    src/buttons.c
    src/diag.c
)

pico_generate_pio_header(claude_handset ${CMAKE_CURRENT_LIST_DIR}/src/i2s_out.pio)
pico_generate_pio_header(claude_handset ${CMAKE_CURRENT_LIST_DIR}/src/pdm_mic.pio)

target_include_directories(claude_handset PRIVATE ${CMAKE_CURRENT_LIST_DIR}/src)

# Route panic() into diag.c so the message and caller survive the watchdog reset.
target_compile_definitions(claude_handset PRIVATE PICO_PANIC_FUNCTION=diag_panic)

target_link_libraries(claude_handset PRIVATE
    pico_stdlib
    pico_unique_id
    pico_bootrom
    hardware_pio
    hardware_dma
    hardware_watchdog
    tinyusb_device
)

# No stdio: the USB interface is the audio + HID composite device, and the
# UART pins are not broken out on the handset.
pico_enable_stdio_uart(claude_handset 0)
pico_enable_stdio_usb(claude_handset 0)

pico_set_program_name(claude_handset "claude-handset")
pico_set_program_description(claude_handset "USB audio (speaker + PDM mic) and F13/F14 HID keyboard handset")
pico_set_program_url(claude_handset "https://github.com/leomancini/claude-handset")

pico_add_extra_outputs(claude_handset)
