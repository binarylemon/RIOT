/*
 * Copyright (C) 2015 Jan Wagner <mail@jwagner.eu>
 *               2016 Freie Universität Berlin
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     cpu_nrf52
 * @{
 *
 * @file
 * @brief       Low-level GPIO driver implementation
 *
 * NOTE: this GPIO driver implementation supports due to hardware limitations
 *       only one pin configured as external interrupt source at a time!
 *
 * @author      Hauke Petersen <hauke.petersen@fu-berlin.de>
 * @author      Jan Wagner <mail@jwagner.eu>
 *
 * @}
 */

#include "cpu.h"
#include "sched.h"
#include "thread.h"
#include "periph/gpio.h"
#include "periph_conf.h"

/**
 * @brief   Place to store the interrupt context
 */
static gpio_isr_ctx_t exti_chan;


int gpio_init(gpio_t pin, gpio_dir_t dir, gpio_pp_t pullup)
{
    /* configure pin direction, input buffer and pull resistor state */
    NRF_P0->PIN_CNF[pin] = ((dir << GPIO_PIN_CNF_DIR_Pos) |
                            (dir << GPIO_PIN_CNF_INPUT_Pos) |
                            (pullup << GPIO_PIN_CNF_PULL_Pos));

    printf("cfg[%i] 0x%08x\n", (int)pin, (unsigned)NRF_P0->PIN_CNF[pin]);
    return 0;
}

int gpio_init_int(gpio_t pin, gpio_pp_t pullup, gpio_flank_t flank,
                  gpio_cb_t cb, void *arg)
{
    /* disable external interrupt in case one is active */
    NRF_GPIOTE->INTENSET &= ~(GPIOTE_INTENSET_IN0_Msk);
    /* save callback */
    exti_chan.cb = cb;
    exti_chan.arg = arg;
    /* configure pin as input */
    gpio_init(pin, GPIO_DIR_IN, pullup);
    /* set interrupt priority and enable global GPIOTE interrupt */
    NVIC_EnableIRQ(GPIOTE_IRQn);
    /* configure the GPIOTE channel: set even mode, pin and active flank */
    NRF_GPIOTE->CONFIG[0] = (GPIOTE_CONFIG_MODE_Event |
                             (pin << GPIOTE_CONFIG_PSEL_Pos) |
                             (flank << GPIOTE_CONFIG_POLARITY_Pos));
    /* enable external interrupt */
    NRF_GPIOTE->INTENSET |= GPIOTE_INTENSET_IN0_Msk;
    return 0;
}

void gpio_irq_enable(gpio_t pin)
{
    (void) pin;
    NRF_GPIOTE->INTENSET |= GPIOTE_INTENSET_IN0_Msk;
}

void gpio_irq_disable(gpio_t pin)
{
    (void) pin;
    NRF_GPIOTE->INTENCLR |= GPIOTE_INTENSET_IN0_Msk;
}

int gpio_read(gpio_t pin)
{
    if (NRF_P0->DIR & (1 << pin)) {
        return (NRF_P0->OUT & (1 << pin)) ? 1 : 0;
    }
    else {
        return (NRF_P0->IN & (1 << pin)) ? 1 : 0;
    }
}

void gpio_set(gpio_t pin)
{
    NRF_P0->OUTSET = (1 << pin);
}

void gpio_clear(gpio_t pin)
{
    NRF_P0->OUTCLR = (1 << pin);
}

void gpio_toggle(gpio_t pin)
{
    NRF_P0->OUT ^= (1 << pin);
}

void gpio_write(gpio_t pin, int value)
{
    if (value) {
        NRF_P0->OUTSET = (1 << pin);
    } else {
        NRF_P0->OUTCLR = (1 << pin);
    }
}

void isr_gpiote(void)
{
    if (NRF_GPIOTE->EVENTS_IN[0] == 1) {
        NRF_GPIOTE->EVENTS_IN[0] = 0;
        exti_chan.cb(exti_chan.arg);
    }
    if (sched_context_switch_request) {
        thread_yield();
    }
}
