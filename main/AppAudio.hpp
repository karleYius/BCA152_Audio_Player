#pragma once
#include <cstdio>
#include <cinttypes>
#include <string>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_err.h"
#include "esp_log.h"
#include "driver/i2s_std.h"
#include "audio_player.h"
#include "Config.hpp"

namespace
{
    void audio_player_callback(audio_player_cb_ctx_t *obj);
}

namespace app
{
    enum class eAudioEvent
    {
        PLAYING,
        STOPPED
    };

    class AppAudio
    {
    private:
        FILE *m_fp = nullptr;
        uint8_t m_vol = 50;
        audio_player_state_t m_player_state = AUDIO_PLAYER_STATE_IDLE;
        QueueHandle_t m_event_queue = nullptr;
        bool m_muted = false;

        static inline i2s_chan_handle_t s_tx = nullptr;
        static inline volatile uint32_t s_gain = 2500;   // volume squared, 0 to 10000
        static inline volatile bool s_muted = false;

        static esp_err_t muteFn(AUDIO_PLAYER_MUTE_SETTING setting)
        {
            return ESP_OK;
        }

        static esp_err_t i2sClk(uint32_t rate, uint32_t bits_cfg, i2s_slot_mode_t ch)
        {
            esp_err_t err = i2s_channel_disable(s_tx);
            if (err != ESP_OK)
            {
                return err;
            }
            i2s_std_clk_config_t clk = I2S_STD_CLK_DEFAULT_CONFIG(rate);
            i2s_std_slot_config_t slot = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
                (i2s_data_bit_width_t)bits_cfg, ch);
            err = i2s_channel_reconfig_std_clock(s_tx, &clk);
            if (err == ESP_OK)
            {
                err = i2s_channel_reconfig_std_slot(s_tx, &slot);
            }
            esp_err_t en = i2s_channel_enable(s_tx);
            return (err != ESP_OK) ? err : en;
        }

        static esp_err_t i2sWrite(const void *buf, size_t len, size_t *written, uint32_t timeout_ms)
        {
            const int16_t *in = reinterpret_cast<const int16_t *>(buf);
            int32_t gain = s_muted ? 0 : (int32_t)s_gain;
            int16_t tmp[256];
            size_t total = len / 2;
            size_t idx = 0;
            size_t done_bytes = 0;
            esp_err_t err = ESP_OK;
            while (idx < total)
            {
                size_t n = total - idx;
                if (n > 256)
                {
                    n = 256;
                }
                for (size_t i = 0; i < n; i++)
                {
                    tmp[i] = (int16_t)(((int32_t)in[idx + i] * gain) / 10000);
                }
                size_t w = 0;
                err = i2s_channel_write(s_tx, tmp, n * 2, &w, timeout_ms);
                done_bytes += w;
                if (err != ESP_OK || w < n * 2)
                {
                    break;
                }
                idx += n;
            }
            if (written != nullptr)
            {
                *written = done_bytes;
            }
            return err;
        }

        void applyVolume() { s_gain = (uint32_t)m_vol * (uint32_t)m_vol; }

    public:
        void init()
        {
            i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
            chan_cfg.auto_clear = true;
            ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &s_tx, NULL));

            i2s_std_config_t std_cfg = {
                .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(44100),
                .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
                .gpio_cfg = {
                    .mclk = I2S_GPIO_UNUSED,
                    .bclk = cfg::I2S_BCLK,
                    .ws = cfg::I2S_LRC,
                    .dout = cfg::I2S_DIN,
                    .din = I2S_GPIO_UNUSED,
                    .invert_flags = {
                        .mclk_inv = false,
                        .bclk_inv = false,
                        .ws_inv = false,
                    },
                },
            };
            ESP_ERROR_CHECK(i2s_channel_init_std_mode(s_tx, &std_cfg));
            ESP_ERROR_CHECK(i2s_channel_enable(s_tx));

            audio_player_config_t config = {};
            config.mute_fn = muteFn;
            config.clk_set_fn = i2sClk;
            config.write_fn = i2sWrite;
            config.priority = 1;
            ESP_ERROR_CHECK(audio_player_new(config));
            audio_player_callback_register(audio_player_callback, this);

            applyVolume();
            m_event_queue = xQueueCreate(10, sizeof(app::eAudioEvent));
        }

        QueueHandle_t getEventQueue(void) const { return m_event_queue; }

        uint8_t mute(bool m, bool toggle = false)
        {
            m_muted = toggle ? !m_muted : m;
            s_muted = m_muted;
            return m_muted ? 0 : m_vol;
        }

        bool isMuted(void) const { return m_muted; }
        uint8_t getVolume(void) const { return m_vol; }

        void togglePlay(const std::string &filename)
        {
            switch (m_player_state)
            {
            case AUDIO_PLAYER_STATE_PLAYING:
                audio_player_pause();
                break;
            case AUDIO_PLAYER_STATE_PAUSE:
                audio_player_resume();
                break;
            default:
                m_fp = fopen(filename.c_str(), "rb");
                if (m_fp == nullptr)
                {
                    ESP_LOGE("audio", "Cannot open %s", filename.c_str());
                    break;
                }
                audio_player_play(m_fp);
                break;
            }
        }

        uint8_t volumeUp(void)
        {
            if (m_vol < 100)
            {
                m_vol += 10;
                applyVolume();
            }
            return m_vol;
        }

        uint8_t volumeDown(void)
        {
            if (m_vol > 0)
            {
                m_vol -= 10;
                applyVolume();
            }
            return m_vol;
        }

        void setState(audio_player_state_t state) { m_player_state = state; }
    };
}

namespace
{
    void audio_player_callback(audio_player_cb_ctx_t *param)
    {
        app::AppAudio &app_audio = *(reinterpret_cast<app::AppAudio *>(param->user_ctx));
        audio_player_state_t state = audio_player_get_state();
        app_audio.setState(state);
        app::eAudioEvent evt = (state == AUDIO_PLAYER_STATE_PLAYING)
                                   ? app::eAudioEvent::PLAYING
                                   : app::eAudioEvent::STOPPED;
        xQueueSend(app_audio.getEventQueue(), (void *)(&evt), 0);
    }
} 