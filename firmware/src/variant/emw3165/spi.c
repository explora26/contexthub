/*
 * Copyright (C) 2016 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <plat/inc/spi.h>
#include <util.h>

static const struct StmSpiBoardCfg mStmSpiBoardCfgs[] = {
    [0] = {
        .gpioMiso = GPIO_PA(12),
        .gpioMosi = GPIO_PA(10),
        .gpioSclk = GPIO_PB(0),
        .gpioNss = GPIO_PB(1),

        .gpioFuncMiso = GPIO_AF_SPI5_B,
        .gpioFuncMosi = GPIO_AF_SPI5_B,
        .gpioFuncSclk = GPIO_AF_SPI5_B,
        .gpioFuncNss = GPIO_AF_SPI5_B,
        .gpioSpeed = GPIO_SPEED_MEDIUM,

        .irqNss = EXTI1_IRQn,

        .dmaRx = SPI5_DMA_RX_CFG_B,
        .dmaTx = SPI5_DMA_TX_CFG_B,

        .sleepDev = -1,
    },
    [1] = {
        .gpioMiso = GPIO_PA(11),
        .gpioMosi = GPIO_PA(1),
        .gpioSclk = GPIO_PB(13),
        .gpioNss = GPIO_PB(12),

        .gpioFuncMiso = GPIO_AF_SPI4_B,
        .gpioFuncMosi = GPIO_AF_SPI4_A,
        .gpioFuncSclk = GPIO_AF_SPI4_B,
        .gpioFuncNss = GPIO_AF_SPI4_B,
        .gpioSpeed = GPIO_SPEED_MEDIUM,

        .irqNss = EXTI15_10_IRQn,

        .dmaRx = SPI4_DMA_RX_CFG_B,
        .dmaTx = SPI4_DMA_TX_CFG_B,

        .sleepDev = Stm32sleepDevSpi2,
    }
};

const struct StmSpiBoardCfg *boardStmSpiCfg(uint8_t busId)
{
    switch (busId) {
        case 3:
        return &mStmSpiBoardCfgs[1];
        case 4:
        return &mStmSpiBoardCfgs[0];
    }

    return NULL;
}
