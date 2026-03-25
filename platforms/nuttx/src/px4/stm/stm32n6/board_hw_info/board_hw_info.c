/****************************************************************************
 * STM32N6 board hardware info stub
 ****************************************************************************/

#include <px4_platform_common/px4_config.h>
#include <stdio.h>
#include <string.h>

__EXPORT const char *board_get_hw_type_name(void)
{
	return "N6_NUCLEO";
}

__EXPORT int board_get_hw_version(void)
{
	return 0;
}

__EXPORT int board_get_hw_revision(void)
{
	return 0;
}

__EXPORT int board_determine_hw_info(void)
{
	return 0;
}
