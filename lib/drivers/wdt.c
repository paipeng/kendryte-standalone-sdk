/* Copyright 2018 Canaan Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "wdt.h"
#include "platform.h"
#include "stddef.h"
#include "utils.h"
#include "sysctl.h"
#include "plic.h"

volatile wdt_t *const wdt[2] =
{
    (volatile wdt_t *)WDT0_BASE_ADDR,
    (volatile wdt_t *)WDT1_BASE_ADDR
};

static void wdt_enable(wdt_device_number_t id)
{
    wdt[id]->crr = WDT_CRR_MASK;
    wdt[id]->cr |= WDT_CR_ENABLE;
}

static void wdt_disable(wdt_device_number_t id)
{
    wdt[id]->crr = WDT_CRR_MASK;
    wdt[id]->cr &= (~WDT_CR_ENABLE);
}

static void wdt_set_timeout(wdt_device_number_t id, uint8_t timeout)
{
    wdt[id]->torr = WDT_TORR_TOP(timeout);
}

static void wdt_response_mode(wdt_device_number_t id, uint8_t mode)
{
    wdt[id]->cr &= (~WDT_CR_RMOD_MASK);
    wdt[id]->cr |= mode;
}

static uint64_t wdt_get_pclk(wdt_device_number_t id)
{
    return id ? sysctl_clock_get_freq(SYSCTL_CLOCK_WDT1) : sysctl_clock_get_freq(SYSCTL_CLOCK_WDT0);
}

static uint64_t wdt_log_2(uint64_t x)
{
    int64_t i = 0;
    for (i = sizeof(uint64_t) * 8; i >= 0; i--)
    {
        if ((x >> i) & 0x1)
        {
            break;
        }
    }
    return i;
}

static uint8_t wdt_get_top(wdt_device_number_t id, uint64_t timeout_ms)
{
    uint64_t wdt_clk = wdt_get_pclk(id);
    uint64_t ret = (timeout_ms * wdt_clk / 1000) >> 16;
    if (ret)
        ret = wdt_log_2(ret);
    if (ret > 0xf)
        ret = 0xf;
    return (uint8_t)ret;
}

void wdt_feed(wdt_device_number_t id)
{
    wdt[id]->crr = WDT_CRR_MASK;
}

void wdt_clear_interrupt(wdt_device_number_t id)
{
    wdt[id]->eoi = wdt[id]->eoi;
}

int wdt_start(wdt_device_number_t id, uint64_t time_out_ms, plic_irq_callback_t on_irq)
{
    wdt_disable(id);
    wdt_clear_interrupt(id);
    plic_irq_register(id ? IRQN_WDT1_INTERRUPT : IRQN_WDT0_INTERRUPT, on_irq, NULL);
    plic_set_priority(id ? IRQN_WDT1_INTERRUPT : IRQN_WDT0_INTERRUPT, 1);
    plic_irq_enable(id ? IRQN_WDT1_INTERRUPT : IRQN_WDT0_INTERRUPT);

    sysctl_reset(id ? SYSCTL_RESET_WDT1 : SYSCTL_RESET_WDT0);
    sysctl_clock_set_threshold(id ? SYSCTL_THRESHOLD_WDT1 : SYSCTL_THRESHOLD_WDT0, 0);
    sysctl_clock_enable(id ? SYSCTL_CLOCK_WDT1 : SYSCTL_CLOCK_WDT0);
    wdt_response_mode(id, WDT_CR_RMOD_INTERRUPT);
    uint8_t m_top = wdt_get_top(id, time_out_ms);
    wdt_set_timeout(id, m_top);
    wdt_enable(id);
    return 0;
}

void wdt_stop(wdt_device_number_t id)
{
    wdt_disable(id);
}
