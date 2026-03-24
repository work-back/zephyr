#ifndef ZEPHYR_DRIVERS_AUDIO_ES8311_H_
#define ZEPHYR_DRIVERS_AUDIO_ES8311_H_

#include <zephyr/types.h>

/* 寄存器定义 */
#define ES8311_REG00_RESET         0x00
#define ES8311_REG01_CLK_MAN       0x01
#define ES8311_REG02_CLK_DIV_M     0x02
#define ES8311_REG03_ADC_OSR       0x03
#define ES8311_REG04_DAC_OSR       0x04
#define ES8311_REG05_CLK_DIV_AD    0x05
#define ES8311_REG06_BCLK_DIV      0x06
#define ES8311_REG07_LRCK_DIV_H    0x07
#define ES8311_REG08_LRCK_DIV_L    0x08
#define ES8311_REG09_SDP_IN        0x09
#define ES8311_REG0A_SDP_OUT       0x0A
#define ES8311_REG0D_SYSTEM        0x0D
#define ES8311_REG0E_SYSTEM        0x0E
#define ES8311_REG12_SYSTEM_DAC    0x12
#define ES8311_REG13_SYSTEM_HP     0x13
#define ES8311_REG14_SYSTEM_PGA    0x14
#define ES8311_REG16_ADC_GAIN      0x16
#define ES8311_REG17_ADC_VOL       0x17
#define ES8311_REG1C_ADC_EQ        0x1C
#define ES8311_REG31_DAC_MUTE      0x31
#define ES8311_REG32_DAC_VOL       0x32
#define ES8311_REG37_DAC_RAMP      0x37

/* 时钟系数结构体 (源自 ESP-IDF) */
struct es8311_coeff {
    uint32_t mclk;
    uint32_t rate;
    uint8_t pre_div;
    uint8_t pre_multi;
    uint8_t adc_div;
    uint8_t dac_div;
    uint8_t fs_mode;
    uint8_t lrck_h;
    uint8_t lrck_l;
    uint8_t bclk_div;
    uint8_t adc_osr;
    uint8_t dac_osr;
};

#endif