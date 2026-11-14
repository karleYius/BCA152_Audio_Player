#pragma once
#include <cstdint>
#include "driver/gpio.h"

namespace cfg
{
    // OLED (I2C)
    constexpr gpio_num_t OLED_SDA = GPIO_NUM_8;
    constexpr gpio_num_t OLED_SCL = GPIO_NUM_9;
    constexpr uint8_t OLED_ADDR = 0x3C;

    // MAX98357 amplifier (I2S)
    constexpr gpio_num_t I2S_BCLK = GPIO_NUM_4;
    constexpr gpio_num_t I2S_LRC = GPIO_NUM_5;
    constexpr gpio_num_t I2S_DIN = GPIO_NUM_6;

    // Buttons (each wired between the pin and GND)
    constexpr gpio_num_t BTN_LEFT = GPIO_NUM_1;
    constexpr gpio_num_t BTN_MID = GPIO_NUM_2;
    constexpr gpio_num_t BTN_RIGHT = GPIO_NUM_42;
}