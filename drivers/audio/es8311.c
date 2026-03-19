#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/audio/codec.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/printk.h>
#include "es8311.h"

LOG_MODULE_REGISTER(everest_es8311, CONFIG_AUDIO_CODEC_LOG_LEVEL);

#define DT_DRV_COMPAT everest_es8311

struct es8311_driver_config {
    struct i2c_dt_spec i2c;
};

/* --- I2C 辅助函数 (8位地址/8位数据) --- */
static int es8311_write_reg(const struct device *dev, uint8_t reg, uint8_t val)
{
    const struct es8311_driver_config *cfg = dev->config;
    return i2c_reg_write_byte_dt(&cfg->i2c, reg, val);
}

static int es8311_read_reg(const struct device *dev, uint8_t reg, uint8_t *val)
{
    const struct es8311_driver_config *cfg = dev->config;
    return i2c_reg_read_byte_dt(&cfg->i2c, reg, val);
}

static int es8311_update_reg(const struct device *dev, uint8_t reg, uint8_t mask, uint8_t val)
{
    uint8_t old_val, new_val;
    int ret = es8311_read_reg(dev, reg, &old_val);
    if (ret != 0) return ret;
    new_val = (old_val & ~mask) | (val & mask);
    return es8311_write_reg(dev, reg, new_val);
}

/* --- 音频配置 --- */
static int es8311_set_dai_fmt(const struct device *dev, audio_dai_cfg_t *cfg)
{
    uint8_t fmt = 0, wl = 0;

    /* 映射格式 (I2S, LJ, etc.) */
    switch (cfg->i2s.format) {
    case I2S_FMT_DATA_FORMAT_I2S: fmt = ES8311_I2S; break;
    case I2S_FMT_DATA_FORMAT_LEFT_JUSTIFIED: fmt = ES8311_LJ; break;
    default: return -EINVAL;
    }

    /* 映射字长 */
    switch (cfg->i2s.word_size) {
    case 16: wl = 3 << 2; break;
    case 24: wl = 0 << 2; break;
    case 32: wl = 4 << 2; break;
    default: return -EINVAL;
    }

    es8311_update_reg(dev, ES8311_REG_SDP_IN, ES8311_SDP_IN_FMT_MASK | ES8311_SDP_IN_WL_MASK, fmt | wl);
    es8311_update_reg(dev, ES8311_REG_SDP_OUT, ES8311_SDP_OUT_FMT_MASK | ES8311_SDP_OUT_WL_MASK, fmt | wl);
    
    return 0;
}

static int es8311_dump_status(const struct device *dev)
{
    uint8_t id1, id2, flag, sys12, sys0d, clk01;

    // 1. 验证通信与芯片 ID (预期: 0x83 0x11)
    es8311_read_reg(dev, ES8311_REG_CHIP_ID1, &id1);
    es8311_read_reg(dev, ES8311_REG_CHIP_ID2, &id2);

    // 2. 读取状态机标志 (Register 0xFC)
    // Bits [6:4] FLAG_CSM_CHIP: 0=S0(Wait), 7=S7(Ready)
    es8311_read_reg(dev, 0xFC, &flag);

    // 3. 读取电源与时钟控制
    es8311_read_reg(dev, 0x01, &clk01); // MCLK_ON, BCLK_ON
    es8311_read_reg(dev, 0x0D, &sys0d); // PDN_ANA (应该为0以使能模拟电路)
    es8311_read_reg(dev, 0x12, &sys12); // PDN_DAC (应该为0以使能DAC)

    printk("--- ES8311 Diagnostic Report ---\n");
    printk("Chip ID: 0x%02X%02X (Expected: 0x8311)\n", id1, id2);
    printk("CSM State: S%d (Expected: S7 for playback)\n", (flag >> 4) & 0x07);
    printk("Clock Config: 0x%02X (MCLK_ON: %d, BCLK_ON: %d)\n",
            clk01, (clk01 >> 5) & 1, (clk01 >> 4) & 1);
    printk("Power Status: DAC_PDN=%d, ANA_PDN=%d\n",
            (sys12 >> 1) & 1, (sys0d >> 7) & 1);

    if (id1 != 0x83) {
        printk("Error: I2C Communication failed or wrong address!\n");
        return -EIO;
    }
    return 0;
}

static int es8311_configure(const struct device *dev, struct audio_codec_cfg *cfg)
{
    int ret;

    /* 1. 软复位 */
    es8311_write_reg(dev, ES8311_REG_RESET, 0x1F); // 这里的复位值参考 DS
    k_msleep(10);
    es8311_write_reg(dev, ES8311_REG_RESET, 0x00);

    /* 2. 时钟与电源初始化 (这里需要根据 Datasheet 的典型上电序列) */
    es8311_write_reg(dev, ES8311_REG_SYSTEM0B, 0x00); // 示例：Power up stage A
    es8311_write_reg(dev, ES8311_REG_CLK_MANAGER01, ES8311_MCLK_ON | ES8311_BCLK_ON); // 开启 MCLK/BCLK

    // 3. 使能模拟电源与使能 DAC
    es8311_write_reg(dev, ES8311_REG_SYSTEM0D, 0x00); // PDN_ANA=0 (Enable analog)
    es8311_write_reg(dev, ES8311_REG_SYSTEM12, 0x00); // PDN_DAC=0 (Enable DAC)
    es8311_write_reg(dev, ES8311_REG_SYSTEM13, 0x10); // HPSW=1 (Enable HP driver for OUTP/N)

    /* 4. 配置 DAI 格式 */
    ret = es8311_set_dai_fmt(dev, &cfg->dai_cfg);
    if (ret != 0) return ret;

    /* 5. 设置采样率 (逻辑较复杂，简化处理) */
    // ES8311 需要根据 MCLK 和 Fs 配置 ADC_OSR (Reg 0x03) 和 DAC_OSR (Reg 0x04)
    // 假设使用默认 256Fs 模式
    
    /* 6. 解除静音 */
    es8311_write_reg(dev, ES8311_REG_ADC_VOLUME, 0xBF); // 0dB
    es8311_write_reg(dev, ES8311_REG_DAC_VOLUME, 0xBF); // 0dB

    /* CSM on */
    es8311_write_reg(dev, ES8311_REG_RESET, ES8311_CSM_ON);

    return 0;
}

static int es8311_set_property(const struct device *dev, audio_property_t property,
                               audio_channel_t channel, audio_property_value_t val)
{
    switch (property) {
    case AUDIO_PROPERTY_OUTPUT_VOLUME:
        /* ES8311 DAC Volume: 0x00 = -95.5dB, 0xBF = 0dB, 0xFF = +32dB */
        return es8311_write_reg(dev, ES8311_REG_DAC_VOLUME, (uint8_t)val.vol);
    case AUDIO_PROPERTY_OUTPUT_MUTE:
        return es8311_update_reg(dev, ES8311_REG_DAC_CONTROL31, 0x40, val.mute ? 0x40 : 0x00);
    default:
        return -ENOTSUP;
    }
}

static const struct audio_codec_api es8311_api = {
    .configure = es8311_configure,
    .set_property = es8311_set_property,
    .dump = es8311_dump_status,
};

#define ES8311_INIT(n)                                         \
    static const struct es8311_driver_config es8311_cfg_##n = { \
        .i2c = I2C_DT_SPEC_INST_GET(n),                        \
    };                                                         \
    DEVICE_DT_INST_DEFINE(n, NULL, NULL, NULL,                  \
                          &es8311_cfg_##n, POST_KERNEL,        \
                          CONFIG_AUDIO_CODEC_INIT_PRIORITY,    \
                          &es8311_api);

DT_INST_FOREACH_STATUS_OKAY(ES8311_INIT)
