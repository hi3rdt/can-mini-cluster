/**
 * @file    adc.c
 * @brief   ADC1 polled driver (see adc.h).
 *
 * ADCCLK = PCLK2 / 6 = 12 MHz (max 14 MHz, prescaler set in rcc.c). Sample time 239.5 cycles gives the
 * potentiometer's high source impedance time to charge the sample capacitor:
 * conversion = 239.5 + 12.5 = 252 cycles = 21 us.
 */
#include "adc.h"
#include "gpio.h"
#include "systick.h"
#include "stm32f1xx.h"

#define ADC_SMP_239_5       (7UL)
#define ADC_EXTSEL_SWSTART  (7UL)
#define ADC_TIMEOUT_MS      (5UL)

static bool wait_flag(volatile const uint32_t *reg, uint32_t mask, bool set)
{
    const uint32_t t0 = tick_get();
    while (((*reg & mask) != 0UL) != set)
    {
        if (tick_elapsed(t0, ADC_TIMEOUT_MS))
        {
            return false;
        }
    }
    return true;
}

std_return_t adc_init(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_ADC1EN;
    gpio_config(GPIOA, 0U, GPIO_IN_ANALOG);

    /* Power up, wait tSTAB (1 us; two SysTicks is plenty), then self-calibrate */
    ADC1->CR2 = ADC_CR2_ADON;
    tick_delay_ms(2UL);
    ADC1->CR2 |= ADC_CR2_RSTCAL;
    if (!wait_flag(&ADC1->CR2, ADC_CR2_RSTCAL, false)) { return E_TIMEOUT; }
    ADC1->CR2 |= ADC_CR2_CAL;
    if (!wait_flag(&ADC1->CR2, ADC_CR2_CAL, false))    { return E_TIMEOUT; }

    ADC1->SMPR2 = ADC_SMP_239_5 << ADC_SMPR2_SMP0_Pos;
    ADC1->SQR1  = 0UL;                              /* 1 conversion in the sequence */
    ADC1->SQR3  = 0UL;                              /* SQ1 = channel 0              */
    /* Software trigger: EXTSEL = SWSTART and EXTTRIG must be set, then SWSTART starts */
    ADC1->CR2   = ADC_CR2_ADON | (ADC_EXTSEL_SWSTART << ADC_CR2_EXTSEL_Pos) | ADC_CR2_EXTTRIG;
    return E_OK;
}

uint16_t adc_read_fuel(void)
{
    ADC1->CR2 |= ADC_CR2_SWSTART;
    if (!wait_flag(&ADC1->SR, ADC_SR_EOC, true))
    {
        return 0U;                                  /* treated as empty tank */
    }
    return (uint16_t)(ADC1->DR & 0x0FFFUL);         /* reading DR clears EOC */
}
