/**
 * @file    tim.c
 * @brief   PWM outputs and input capture (see tim.h).
 */
#include "tim.h"
#include "gpio.h"
#include "systick.h"
#include "board.h"

#define BACKLIGHT_PWM_HZ    (20000UL)
#define BACKLIGHT_ARR       ((PCLK2_HZ / BACKLIGHT_PWM_HZ) - 1UL)   /* TIM1 on APB2, 72 MHz */
#define CAPTURE_TICK_HZ     (1000000UL)

/* ----------------------------------------------------------------- TIM1 backlight */
void tim_backlight_init(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_TIM1EN;
    gpio_config(GPIOA, 8U, GPIO_AF_PP_50MHZ);

    TIM1->PSC   = 0U;
    TIM1->ARR   = BACKLIGHT_ARR;
    TIM1->CCR1  = BACKLIGHT_ARR + 1UL;                  /* 100 % */
    TIM1->CCMR1 = (6UL << TIM_CCMR1_OC1M_Pos) | TIM_CCMR1_OC1PE;   /* PWM mode 1 */
    TIM1->CCER  = TIM_CCER_CC1E;
    TIM1->BDTR  = TIM_BDTR_MOE;     /* advanced timer: outputs stay off without MOE */
    TIM1->EGR   = TIM_EGR_UG;
    TIM1->CR1   = TIM_CR1_ARPE | TIM_CR1_CEN;
}

void tim_backlight_set(uint8_t percent)
{
    const uint32_t p = (percent > 100U) ? 100UL : (uint32_t)percent;
    TIM1->CCR1 = ((BACKLIGHT_ARR + 1UL) * p) / 100UL;
}

/* ----------------------------------------------------------------- TIM4 buzzer */
void tim_buzzer_init(uint32_t tone_hz)
{
    RCC->APB1ENR |= RCC_APB1ENR_TIM4EN;
    gpio_config(GPIOB, 6U, GPIO_AF_PP_50MHZ);

    TIM4->PSC   = (uint16_t)((TIMCLK_APB1_HZ / 1000000UL) - 1UL);  /* 1 MHz */
    TIM4->ARR   = (uint16_t)((1000000UL / tone_hz) - 1UL);
    TIM4->CCR1  = 0U;
    TIM4->CCMR1 = (6UL << TIM_CCMR1_OC1M_Pos) | TIM_CCMR1_OC1PE;
    TIM4->CCER  = TIM_CCER_CC1E;
    TIM4->EGR   = TIM_EGR_UG;
    TIM4->CR1   = TIM_CR1_ARPE | TIM_CR1_CEN;
}

void tim_buzzer_on(bool on)
{
    TIM4->CCR1 = on ? ((TIM4->ARR + 1UL) / 2UL) : 0UL;
}

/* ----------------------------------------------------------------- TIM2 capture */
static volatile uint32_t s_overflows;      /* extends the 16-bit counter to 32 bit */
static volatile uint32_t s_last_capture;
static volatile uint32_t s_period_us;
static volatile uint32_t s_last_edge_ms;
static volatile uint8_t  s_edges;          /* saturates at 2 */

void tim_capture_init(void)
{
    RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;
    gpio_config(GPIOA, 1U, GPIO_IN_FLOATING);

    TIM2->PSC   = (uint16_t)((TIMCLK_APB1_HZ / CAPTURE_TICK_HZ) - 1UL);  /* 1 us tick */
    TIM2->ARR   = 0xFFFFU;
    TIM2->CCMR1 = (1UL << TIM_CCMR1_CC2S_Pos)     /* CC2 <- TI2 */
                | (3UL << TIM_CCMR1_IC2F_Pos);    /* digital filter: 8 samples @ fCK_INT */
    TIM2->CCER  = TIM_CCER_CC2E;                  /* rising edge */
    TIM2->DIER  = TIM_DIER_CC2IE | TIM_DIER_UIE;
    TIM2->EGR   = TIM_EGR_UG;
    TIM2->SR    = 0U;
    TIM2->CR1   = TIM_CR1_CEN;

    NVIC_SetPriority(TIM2_IRQn, IRQ_PRIO_TIM2);
    NVIC_EnableIRQ(TIM2_IRQn);
}

bool tim_capture_get(uint32_t *period_us, uint32_t *last_edge_ms)
{
    __disable_irq();                    /* two words must be read consistently */
    const uint32_t p = s_period_us;
    const uint32_t t = s_last_edge_ms;
    const uint8_t  e = s_edges;
    __enable_irq();

    *period_us    = p;
    *last_edge_ms = t;
    return e >= 2U;
}

void TIM2_IRQHandler(void)
{
    const uint32_t sr = TIM2->SR;
    uint32_t overflows = s_overflows;

    if ((sr & TIM_SR_CC2IF) != 0UL)
    {
        const uint32_t ccr = TIM2->CCR2;     /* reading CCR2 clears CC2IF */

        /* Capture and overflow pending together: if the captured value is in the lower
           half, the edge happened after the wrap, so count the overflow first. */
        if (((sr & TIM_SR_UIF) != 0UL) && (ccr < 0x8000UL))
        {
            overflows++;
            TIM2->SR = ~TIM_SR_UIF;          /* rc_w0: write 0 only to UIF */
            s_overflows = overflows;
        }

        const uint32_t now = (overflows << 16) | ccr;
        if (s_edges > 0U)
        {
            s_period_us = now - s_last_capture;
        }
        s_last_capture = now;
        s_last_edge_ms = tick_get();
        if (s_edges < 2U)
        {
            s_edges++;
        }
    }

    if ((TIM2->SR & TIM_SR_UIF) != 0UL)
    {
        TIM2->SR = ~TIM_SR_UIF;
        s_overflows = overflows + 1UL;
    }
}
