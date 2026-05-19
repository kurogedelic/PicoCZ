add_executable(PicoCZ
    main.cpp
    usb_descriptors.c
)

# Pull in CrispyZebra.h from your submodule path
target_include_directories(PicoCZ PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../submodules/CrispyZebra
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Link essential hardware libraries and TinyUSB device support
target_link_libraries(PicoCZ
    pico_stdlib
    hardware_pio
    hardware_dma
    hardware_irq
    tinyusb_device
    tinyusb_board
    pico_audio_i2s
)

# Enable USB, disable UART for standard operation
pico_enable_stdio_usb(PicoCZ 1)
pico_enable_stdio_uart(PicoCZ 0)

# Create extra build artifacts (bin, hex, uf2)
pico_add_extra_outputs(PicoCZ)