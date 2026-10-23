/**
 * @file    fault.c
 * @brief   HardFault / MemManage / BusFault / UsageFault handler with register dump,
 *          and NMI handler for the clock security system.
 *
 * The naked entry picks MSP or PSP from EXC_RETURN bit 2 and passes the stacked
 * frame {r0, r1, r2, r3, r12, lr, pc, xpsr} to C. Output is polled UART because
 * interrupts and the scheduler can no longer be trusted. Then: reset.
 */
#include "uart.h"
#include "stm32f1xx.h"

void fault_report(const uint32_t *frame);

static void put_hex(const char *name, uint32_t v)
{
    static const char hex[] = "0123456789ABCDEF";
    char line[20];
    uint8_t n = 0U;

    while ((n < 6U) && (name[n] != '\0'))
    {
        line[n] = name[n];
        n++;
    }
    line[n++] = '=';
    for (int8_t s = 28; s >= 0; s -= 4)
    {
        line[n++] = hex[(v >> (uint32_t)s) & 0xFUL];
    }
    line[n++] = '\r';
    line[n++] = '\n';
    line[n]   = '\0';
    uart_write_polled(line);
}

void fault_report(const uint32_t *frame)
{
    uart_write_polled("\r\n*** FAULT ***\r\n");
    put_hex("PC", frame[6]);
    put_hex("LR", frame[5]);
    put_hex("xPSR", frame[7]);
    put_hex("R0", frame[0]);
    put_hex("CFSR", SCB->CFSR);     /* MMFSR | BFSR | UFSR */
    put_hex("HFSR", SCB->HFSR);
    put_hex("MMFAR", SCB->MMFAR);
    put_hex("BFAR", SCB->BFAR);
    uart_write_polled("resetting...\r\n");
    NVIC_SystemReset();
}

__attribute__((naked)) void HardFault_Handler(void)
{
    __asm volatile(
        "tst lr, #4      \n"
        "ite eq          \n"
        "mrseq r0, msp   \n"
        "mrsne r0, psp   \n"
        "b fault_report  \n");
}

__attribute__((naked)) void MemManage_Handler(void)  { __asm volatile("b HardFault_Handler"); }
__attribute__((naked)) void BusFault_Handler(void)   { __asm volatile("b HardFault_Handler"); }
__attribute__((naked)) void UsageFault_Handler(void) { __asm volatile("b HardFault_Handler"); }

void NMI_Handler(void)
{
    if ((RCC->CIR & RCC_CIR_CSSF) != 0UL)
    {
        RCC->CIR |= RCC_CIR_CSSC;   /* HSE lost: hardware already switched to HSI */
        uart_write_polled("\r\n*** HSE failure (CSS) ***\r\n");
        NVIC_SystemReset();         /* re-run clock init: fails over to safe state */
    }
}

void fault_enable_traps(void)
{
    SCB->CCR  |= SCB_CCR_DIV_0_TRP_Msk;                     /* divide by zero -> UsageFault */
    SCB->SHCSR |= SCB_SHCSR_MEMFAULTENA_Msk | SCB_SHCSR_BUSFAULTENA_Msk |
                  SCB_SHCSR_USGFAULTENA_Msk;
}
