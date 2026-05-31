# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.2.0)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.2.0-a4)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico2_w CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(hello_pwm C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1

add_executable(hello_pwm
        hello_pwm.c
        dhcpserver.c
        audio_processor.c 
        buton_mute.c
        buton_active.c
        audio_effects.c
        )

#genereaza header-ul PIO
pico_generate_pio_header(hello_pwm ${CMAKE_CURRENT_LIST_DIR}/audio_i2s.pio)


# pull in common dependencies and additional pwm hardware support
target_link_libraries(hello_pwm pico_stdlib pico_stdio_usb  pico_cyw43_arch_lwip_threadsafe_background hardware_flash hardware_sync  hardware_watchdog pico_lwip_http
    pico_lwip_mdns  pico_multicore hardware_adc hardware_pio hardware_pwm hardware_dma)



# pentru fisiere .h din directorul curent
target_include_directories(hello_pwm PRIVATE ${CMAKE_CURRENT_LIST_DIR})

pico_enable_stdio_usb(hello_pwm 1) #Mesajele apar în consola de pe PC via cablu USB.
pico_enable_stdio_uart(hello_pwm 0)

# create map/bin/hex file etc.
pico_add_extra_outputs(hello_pwm)

# add url via pico_set_program_url
