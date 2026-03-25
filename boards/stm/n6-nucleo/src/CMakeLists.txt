# PX4-specific stubs + NuttX board source files
add_library(drivers_board
	# PX4 stubs (flash params, latency, GUID)
	init.c

	# NuttX board layer (peripheral registration)
	stm32_boot.c
	stm32_bringup.c
	stm32_appinit.c
	stm32_autoleds.c
	stm32_userleds.c
	stm32_buttons.c
	stm32_adc.c
	stm32_pwm.c
	stm32_usbdev.c
	stm32_xspi.c

	# PX4 bus configuration
	spi.cpp
	i2c.cpp
)

target_include_directories(drivers_board PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(drivers_board PRIVATE __KERNEL__)

target_link_libraries(drivers_board
	PRIVATE
		nuttx_arch
		nuttx_drivers
		px4_layer
)
