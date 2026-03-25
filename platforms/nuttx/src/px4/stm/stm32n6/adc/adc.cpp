/****************************************************************************
 * STM32N6 ADC platform interface — stub for initial bringup
 ****************************************************************************/

#include <board_config.h>
#include <stdint.h>
#include <drivers/drv_adc.h>
#include <px4_arch/adc.h>

int px4_arch_adc_init(uint32_t base_address)
{
	/* TODO: implement STM32N6 ADC init */
	return 0;
}

void px4_arch_adc_uninit(uint32_t base_address)
{
}

uint32_t px4_arch_adc_sample(uint32_t base_address, unsigned channel)
{
	return UINT32_MAX; /* no sample available */
}

float px4_arch_adc_reference_v()
{
	return 3.3f;
}

uint32_t px4_arch_adc_temp_sensor_mask()
{
	return 0;
}

uint32_t px4_arch_adc_dn_fullcount()
{
	return (1 << 12); /* 12-bit ADC */
}
