/**
 * @file    adc.h
 * @brief   ADC1 continuous scan (IN0 fuel pot, IN16 die temperature) into RAM via DMA1_Channel1.
 */
#ifndef ADC_H
#define ADC_H

#include "std_types.h"

typedef enum
{
    ADC_CH_FUEL = 0,
    ADC_CH_TEMP,
    ADC_CH_COUNT
} adc_ch_t;

std_return_t adc_init(void);
uint16_t     adc_raw(adc_ch_t ch);          /* latest 12-bit sample, no CPU cost */
int16_t      adc_mcu_temp_x10(void);        /* die temperature in 0.1 degC (+-1.5 degC typ.) */

#endif /* ADC_H */
