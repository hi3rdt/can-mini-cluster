/*
 * Linker script for STM32F103C8T6 (64 KB Flash, 20 KB SRAM).
 * The last 1 KB Flash page (0x0800FC00) is reserved for the NVM record store.
 */
ENTRY(Reset_Handler)

MEMORY
{
    FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 63K
    NVM   (r)   : ORIGIN = 0x0800FC00, LENGTH = 1K
    RAM   (rwx) : ORIGIN = 0x20000000, LENGTH = 20K
}

_estack         = ORIGIN(RAM) + LENGTH(RAM);
_min_stack_size = 0x800;   /* 2 KB, checked below */

SECTIONS
{
    .isr_vector :
    {
        . = ALIGN(4);
        KEEP(*(.isr_vector))
        . = ALIGN(4);
    } > FLASH

    .text :
    {
        . = ALIGN(4);
        *(.text*)
        *(.rodata*)
        . = ALIGN(4);
        _etext = .;
    } > FLASH

    .ARM.exidx :
    {
        *(.ARM.exidx*)
    } > FLASH

    _sidata = LOADADDR(.data);

    .data :
    {
        . = ALIGN(4);
        _sdata = .;
        *(.data*)
        . = ALIGN(4);
        _edata = .;
    } > RAM AT > FLASH

    .bss (NOLOAD) :
    {
        . = ALIGN(4);
        _sbss = .;
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        _ebss = .;
    } > RAM

    /* No heap: malloc is not used (MISRA C:2012 Rule 21.3) */
    ._stack_check (NOLOAD) :
    {
        . = ALIGN(8);
        . = . + _min_stack_size;
    } > RAM
}
