/**
 * @file    adc.h
 * @brief   ADC1 single conversion on PA0 (IN0, fuel level potentiometer), polled.
 */
#ifndef ADC_H
#define ADC_H

#include "std_types.h"

#define ADC_RAW_MAX     (4095U)     /* 12-bit result */

std_return_t adc_init(void);
/** Start one conversion and wait for it (~21 us). Returns the 12-bit result. */
uint16_t     adc_read_fuel(void);

#endif /* ADC_H */
