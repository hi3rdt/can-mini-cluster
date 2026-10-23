/**
 * @file    adc.c
 * @brief   ADC1 scan + DMA circular (see adc.h).
 *
 * ADCCLK = 12 MHz, sample time 239.5 cycles (the temperature sensor needs >= 17.1 us),
 * conversion = 239.5 + 12.5 = 252 cycles = 21 us per channel. The DMA keeps
 * s_samples[] refreshed every 42 us with zero interrupts.
 */
#include "adc.h"
#include "gpio.h"
#include "systick.h"
#include "stm32f1xx.h"

#define ADC_SMP_239_5       (7UL)
#define ADC_EXTSEL_SWSTART  (7UL)
#define ADC_CAL_TIMEOUT_MS  (5UL)

/* DS5319: V25 = 1.43 V typ., avg slope = 4.3 mV/degC */
#define TEMP_V25_MV         (1430L)
#define TEMP_SLOPE_UV_PER_C (4300L)
#define VDDA_MV             (3300L)

static volatile uint16_t s_samples[ADC_CH_COUNT];

static bool wait_clear(volatile const uint32_t *reg, uint32_t mask)
{
    const uint32_t t0 = tick_get();
    while ((*reg & mask) != 0UL)
    {
        if (tick_elapsed(t0, ADC_CAL_TIMEOUT_MS))
        {
            return false;
        }
    }
    return true;
}

std_return_t adc_init(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_ADC1EN;
    RCC->AHBENR  |= RCC_AHBENR_DMA1EN;
    gpio_config(GPIOA, 0U, GPIO_IN_ANALOG);

    /* Power up, wait tSTAB (1 us) - two SysTicks is plenty - then calibrate */
    ADC1->CR2 = ADC_CR2_ADON;
    tick_delay_ms(2UL);
    ADC1->CR2 |= ADC_CR2_RSTCAL;
    if (!wait_clear(&ADC1->CR2, ADC_CR2_RSTCAL)) { return E_TIMEOUT; }
    ADC1->CR2 |= ADC_CR2_CAL;
    if (!wait_clear(&ADC1->CR2, ADC_CR2_CAL))    { return E_TIMEOUT; }

    ADC1->SMPR2 = ADC_SMP_239_5 << ADC_SMPR2_SMP0_Pos;
    ADC1->SMPR1 = ADC_SMP_239_5 << ADC_SMPR1_SMP16_Pos;
    ADC1->SQR1  = (uint32_t)(ADC_CH_COUNT - 1U) << ADC_SQR1_L_Pos;   /* 2 conversions */
    ADC1->SQR3  = (0UL << ADC_SQR3_SQ1_Pos) | (16UL << ADC_SQR3_SQ2_Pos);
    ADC1->CR1   = ADC_CR1_SCAN;

    DMA1_Channel1->CCR   = 0UL;
    DMA1_Channel1->CPAR  = (uint32_t)&ADC1->DR;
    DMA1_Channel1->CMAR  = (uint32_t)&s_samples[0];
    DMA1_Channel1->CNDTR = ADC_CH_COUNT;
    DMA1_Channel1->CCR   = DMA_CCR_MINC | DMA_CCR_CIRC | DMA_CCR_PSIZE_0 | DMA_CCR_MSIZE_0 |
                           DMA_CCR_PL_0 | DMA_CCR_EN;          /* 16/16 bit, medium priority */

    ADC1->CR2 = ADC_CR2_ADON | ADC_CR2_CONT | ADC_CR2_DMA | ADC_CR2_TSVREFE |
                (ADC_EXTSEL_SWSTART << ADC_CR2_EXTSEL_Pos) | ADC_CR2_EXTTRIG;
    ADC1->CR2 |= ADC_CR2_SWSTART;
    return E_OK;
}

uint16_t adc_raw(adc_ch_t ch)
{
    return (ch < ADC_CH_COUNT) ? s_samples[ch] : 0U;   /* 16-bit load is atomic */
}

int16_t adc_mcu_temp_x10(void)
{
    const int32_t mv = ((int32_t)adc_raw(ADC_CH_TEMP) * VDDA_MV) / 4095L;
    /* T = (V25 - V) / slope + 25, computed in 0.1 degC with integer math */
    const int32_t t_x10 = (((TEMP_V25_MV - mv) * 10000L) / TEMP_SLOPE_UV_PER_C) + 250L;
    return (int16_t)t_x10;
}
