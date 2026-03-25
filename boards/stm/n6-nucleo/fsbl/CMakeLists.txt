#
# STM32N6 FSBL Build Configuration  
#

# FSBL is a standalone binary, not a PX4 module
add_executable(fsbl_stm32n6
    fsbl_main.c
)

# Set target properties for FSBL
set_target_properties(fsbl_stm32n6 PROPERTIES
    LINK_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/../nuttx-config/scripts/fsbl.ld
)

# Use FSBL-specific linker script
target_link_options(fsbl_stm32n6 PRIVATE
    -T ${CMAKE_CURRENT_SOURCE_DIR}/../nuttx-config/scripts/fsbl.ld
    -Wl,-Map=fsbl.map
    -nostdlib
    -nostartfiles
)

# Compiler flags for FSBL (bare metal)
target_compile_options(fsbl_stm32n6 PRIVATE
    -mcpu=cortex-m7
    -mthumb
    -mfpu=fpv5-d16
    -mfloat-abi=hard
    -ffunction-sections
    -fdata-sections
    -fno-common
    -fno-builtin
    -Os
    -DSTM32N6
)

# Include path for STM32N6 definitions
target_include_directories(fsbl_stm32n6 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

# Generate binary file from ELF
add_custom_command(TARGET fsbl_stm32n6 POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:fsbl_stm32n6> fsbl_stm32n6.bin
    COMMENT "Creating FSBL binary"
)

# Install FSBL binary
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/fsbl_stm32n6.bin 
        DESTINATION bin
        RENAME fsbl.bin
)