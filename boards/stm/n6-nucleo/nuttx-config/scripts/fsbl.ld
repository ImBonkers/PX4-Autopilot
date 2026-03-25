/****************************************************************************
 * STM32N6 FSBL Linker Script
 ****************************************************************************/

MEMORY
{
    /* FSBL loaded to AXISRAM2 by Boot ROM - matches STM32 reference */
    ROM (xrw) : ORIGIN = 0x34180400, LENGTH = 255K
    RAM (xrw) : ORIGIN = 0x341C0000, LENGTH = 256K
}

SECTIONS
{
    /* FSBL header must be at offset 0 */
    .fsbl_header : {
        KEEP(*(.fsbl_header))
    } > ROM
    
    /* Code starts at offset 0x200 as specified in header */
    . = 0x34180600;
    
    .text : {
        *(.text.fsbl_main)
        *(.text*)
        *(.rodata*)
    } > ROM
    
    .data : {
        *(.data*)
    } > RAM AT> ROM
    
    .bss : {
        __bss_start = .;
        *(.bss*)
        *(COMMON)
        __bss_end = .;
    } > RAM
    
    /DISCARD/ : {
        *(.ARM.exidx*)
        *(.ARM.extab*)
    }
}