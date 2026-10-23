/**
 * @file    startup_stm32f103.c
 * @brief   Vector table and reset handler for STM32F103x8/xB (medium density), written in C.
 *
 * Reset_Handler: copy .data from Flash to RAM, zero .bss, then call main().
 * Unused interrupt handlers are weak aliases of Default_Handler, so a driver
 * overrides one just by defining a function with the same name.
 */
#include <stdint.h>
#include "stm32f1xx.h"

extern uint32_t _sidata, _sdata, _edata, _sbss, _ebss, _estack;
extern int main(void);

void Reset_Handler(void);
void Default_Handler(void);

/* Cortex-M3 core exceptions */
void NMI_Handler(void)        __attribute__((weak, alias("Default_Handler")));
void HardFault_Handler(void)  __attribute__((weak, alias("Default_Handler")));
void MemManage_Handler(void)  __attribute__((weak, alias("Default_Handler")));
void BusFault_Handler(void)   __attribute__((weak, alias("Default_Handler")));
void UsageFault_Handler(void) __attribute__((weak, alias("Default_Handler")));
void SVC_Handler(void)        __attribute__((weak, alias("Default_Handler")));
void DebugMon_Handler(void)   __attribute__((weak, alias("Default_Handler")));
void PendSV_Handler(void)     __attribute__((weak, alias("Default_Handler")));
void SysTick_Handler(void)    __attribute__((weak, alias("Default_Handler")));

/* STM32F103 medium-density peripheral interrupts (RM0008 Table 63) */
#define WEAK_IRQ(name) void name(void) __attribute__((weak, alias("Default_Handler")))
WEAK_IRQ(WWDG_IRQHandler);          WEAK_IRQ(PVD_IRQHandler);
WEAK_IRQ(TAMPER_IRQHandler);        WEAK_IRQ(RTC_IRQHandler);
WEAK_IRQ(FLASH_IRQHandler);         WEAK_IRQ(RCC_IRQHandler);
WEAK_IRQ(EXTI0_IRQHandler);         WEAK_IRQ(EXTI1_IRQHandler);
WEAK_IRQ(EXTI2_IRQHandler);         WEAK_IRQ(EXTI3_IRQHandler);
WEAK_IRQ(EXTI4_IRQHandler);         WEAK_IRQ(DMA1_Channel1_IRQHandler);
WEAK_IRQ(DMA1_Channel2_IRQHandler); WEAK_IRQ(DMA1_Channel3_IRQHandler);
WEAK_IRQ(DMA1_Channel4_IRQHandler); WEAK_IRQ(DMA1_Channel5_IRQHandler);
WEAK_IRQ(DMA1_Channel6_IRQHandler); WEAK_IRQ(DMA1_Channel7_IRQHandler);
WEAK_IRQ(ADC1_2_IRQHandler);        WEAK_IRQ(USB_HP_CAN1_TX_IRQHandler);
WEAK_IRQ(USB_LP_CAN1_RX0_IRQHandler); WEAK_IRQ(CAN1_RX1_IRQHandler);
WEAK_IRQ(CAN1_SCE_IRQHandler);      WEAK_IRQ(EXTI9_5_IRQHandler);
WEAK_IRQ(TIM1_BRK_IRQHandler);      WEAK_IRQ(TIM1_UP_IRQHandler);
WEAK_IRQ(TIM1_TRG_COM_IRQHandler);  WEAK_IRQ(TIM1_CC_IRQHandler);
WEAK_IRQ(TIM2_IRQHandler);          WEAK_IRQ(TIM3_IRQHandler);
WEAK_IRQ(TIM4_IRQHandler);          WEAK_IRQ(I2C1_EV_IRQHandler);
WEAK_IRQ(I2C1_ER_IRQHandler);       WEAK_IRQ(I2C2_EV_IRQHandler);
WEAK_IRQ(I2C2_ER_IRQHandler);       WEAK_IRQ(SPI1_IRQHandler);
WEAK_IRQ(SPI2_IRQHandler);          WEAK_IRQ(USART1_IRQHandler);
WEAK_IRQ(USART2_IRQHandler);        WEAK_IRQ(USART3_IRQHandler);
WEAK_IRQ(EXTI15_10_IRQHandler);     WEAK_IRQ(RTC_Alarm_IRQHandler);
WEAK_IRQ(USBWakeUp_IRQHandler);

typedef void (*vector_t)(void);

__attribute__((section(".isr_vector"), used))
const vector_t g_vector_table[16 + 43] = {
    (vector_t)&_estack,
    Reset_Handler,
    NMI_Handler,
    HardFault_Handler,
    MemManage_Handler,
    BusFault_Handler,
    UsageFault_Handler,
    0, 0, 0, 0,
    SVC_Handler,
    DebugMon_Handler,
    0,
    PendSV_Handler,
    SysTick_Handler,
    /* IRQ 0..42 */
    WWDG_IRQHandler, PVD_IRQHandler, TAMPER_IRQHandler, RTC_IRQHandler,
    FLASH_IRQHandler, RCC_IRQHandler, EXTI0_IRQHandler, EXTI1_IRQHandler,
    EXTI2_IRQHandler, EXTI3_IRQHandler, EXTI4_IRQHandler, DMA1_Channel1_IRQHandler,
    DMA1_Channel2_IRQHandler, DMA1_Channel3_IRQHandler, DMA1_Channel4_IRQHandler,
    DMA1_Channel5_IRQHandler, DMA1_Channel6_IRQHandler, DMA1_Channel7_IRQHandler,
    ADC1_2_IRQHandler, USB_HP_CAN1_TX_IRQHandler, USB_LP_CAN1_RX0_IRQHandler,
    CAN1_RX1_IRQHandler, CAN1_SCE_IRQHandler, EXTI9_5_IRQHandler,
    TIM1_BRK_IRQHandler, TIM1_UP_IRQHandler, TIM1_TRG_COM_IRQHandler, TIM1_CC_IRQHandler,
    TIM2_IRQHandler, TIM3_IRQHandler, TIM4_IRQHandler, I2C1_EV_IRQHandler,
    I2C1_ER_IRQHandler, I2C2_EV_IRQHandler, I2C2_ER_IRQHandler, SPI1_IRQHandler,
    SPI2_IRQHandler, USART1_IRQHandler, USART2_IRQHandler, USART3_IRQHandler,
    EXTI15_10_IRQHandler, RTC_Alarm_IRQHandler, USBWakeUp_IRQHandler
};

void Reset_Handler(void)
{
    uint32_t *src = &_sidata;
    uint32_t *dst = &_sdata;

    while (dst < &_edata)
    {
        *dst++ = *src++;
    }
    for (dst = &_sbss; dst < &_ebss; dst++)
    {
        *dst = 0UL;
    }

    (void)main();
    for (;;)
    {
        /* main must not return */
    }
}

void Default_Handler(void)
{
    /* Unexpected interrupt: IPSR[8:0] holds the exception number (debugger: p/x $xpsr) */
    for (;;)
    {
    }
}
