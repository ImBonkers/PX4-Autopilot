/****************************************************************************
 * STM32N6 MCU version stub
 ****************************************************************************/

#include <px4_platform_common/px4_config.h>
#include <px4_platform_common/defines.h>

int board_mcu_version(char *rev, const char **revstr, const char **errata)
{
	*rev = 'A';
	*revstr = "STM32N657X0";

	if (errata) {
		*errata = NULL;
	}

	return 0;
}
