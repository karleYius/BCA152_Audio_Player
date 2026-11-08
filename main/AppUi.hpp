#pragma once
#include <cstdio>
#include <mutex>
#include <string>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "Config.hpp"
#include "Ssd1306.hpp"
#include "AppButton.hpp"
#include "AppNav.hpp"
#include "AppAudio.hpp"

namespace
{
    app::AppNav m_nav;
}

namespace app
{
    class AppUi
    {
    private:
        std::mutex m_ui_access;
        Ssd1306 m_oled;
        bool m_playlist_active = true;
        bool m_playing = false;
        bool m_muted = false;
        bool m_left_down = false;
        bool m_right_down = false;
        int m_volume = 50;
        AppAudio *m_app_audio = nullptr;
        AppButton *m_app_button = nullptr;

        static std::string makeAudioPath(const std::string &filename)
        {
            return std::string("/spiffs/") + filename;
        }

        static void buttonEventTask(void *param)
        {
            AppUi &ui = *(reinterpret_cast<AppUi *>(param));
            eBtnEvent evt;
            while (true)
            {
                xQueueReceive(ui.m_app_button->getEventQueue(), &evt, portMAX_DELAY);
                ui.handleButtonEvent(evt);
            }
        }

        static void audioEventTask(void *param)
        {
            AppUi &ui = *(reinterpret_cast<AppUi *>(param));
            eAudioEvent evt;
            while (true)
            {
                xQueueReceive(ui.m_app_audio->getEventQueue(), &evt, portMAX_DELAY);
                ui.handleAudioEvent(evt);
            }
        }

        void drawKey(int x, const char *label, bool down)
        {
            if (down)
            {
                m_oled.fillRect(x, 48, 18, 16, true);
                m_oled.drawText(x + 4, 50, label, 2, false);
            }
            else
            {
                m_oled.drawText(x + 4, 50, label, 2, true);
            }
        }

        void render()
        {
            m_oled.clear();

            // Volume row
            m_oled.drawText(2, 2, "-", 1, true);
            m_oled.drawRect(14, 0, 100, 11, true);
            if (!m_playlist_active)
            {
                m_oled.drawRect(15, 1, 98, 9, true);
            }
            if (m_muted)
            {
                const char *t = "MUTE";
                int w = m_oled.textWidth(t, 1);
                m_oled.drawText(14 + (100 - w) / 2, 2, t, 1, true);
            }
            else
            {
                int fill = 94 * m_volume / 100;
                m_oled.fillRect(17, 3, fill, 5, true);
            }
            m_oled.drawText(119, 2, "+", 1, true);

            // Animal name and volume number
            const Animal_t &an = m_nav.getCurrent();
            int w = m_oled.textWidth(an.animal.c_str(), 2);
            m_oled.drawText((128 - w) / 2, 18, an.animal.c_str(), 2, true);

            char buf[16];
            snprintf(buf, sizeof(buf), "Vol %d%%", m_volume);
            int vw = m_oled.textWidth(buf, 1);
            m_oled.drawText((128 - vw) / 2, 38, buf, 1, true);

            // Bottom row: Prev, Play/Pause, Next
            const char *label = m_playing ? "Pause" : "Play";
            int lw = m_oled.textWidth(label, 2);
            if (m_playlist_active)
            {
                m_oled.fillRect(26, 48, 76, 16, true);
                m_oled.drawText(26 + (76 - lw) / 2, 50, label, 2, false);
            }
            else
            {
                m_oled.drawRect(26, 48, 76, 16, true);
                m_oled.drawText(26 + (76 - lw) / 2, 50, label, 2, true);
            }
            drawKey(2, "<", m_left_down);
            drawKey(108, ">", m_right_down);

            m_oled.flush();
        }

    public:
        void init(AppButton *button, AppAudio *audio)
        {
            m_app_button = button;
            m_app_audio = audio;
            m_oled.init(cfg::OLED_SDA, cfg::OLED_SCL, cfg::OLED_ADDR);
            m_nav.init();
            m_volume = m_app_audio->getVolume();
            render();

            xTaskCreate(buttonEventTask, "btn_evt", 4 * 1024, this, 3, nullptr);
            xTaskCreate(audioEventTask, "audio_evt", 4 * 1024, this, 3, nullptr);
        }

        void handleButtonEvent(app::eBtnEvent &btn_evt)
        {
            std::lock_guard<std::mutex> lock(m_ui_access);
            switch (btn_evt)
            {
            case app::eBtnEvent::M_DCLICK:
                m_playlist_active = !m_playlist_active;
                break;

            case app::eBtnEvent::M_CLICK:
                if (m_playlist_active)
                {
                    m_app_audio->togglePlay(makeAudioPath(m_nav.getCurrent().audio));
                }
                else
                {
                    m_app_audio->mute(false, true);
                    m_muted = m_app_audio->isMuted();
                }
                break;

            case app::eBtnEvent::L_PRESSED:
                m_left_down = true;
                break;

            case app::eBtnEvent::L_RELEASED:
                m_left_down = false;
                if (m_playlist_active)
                {
                    m_nav.prev();
                }
                else
                {
                    m_volume = m_app_audio->volumeDown();
                }
                break;

            case app::eBtnEvent::R_PRESSED:
                m_right_down = true;
                break;

            case app::eBtnEvent::R_RELEASED:
                m_right_down = false;
                if (m_playlist_active)
                {
                    m_nav.next();
                }
                else
                {
                    m_volume = m_app_audio->volumeUp();
                }
                break;

            default:
                break;
            }
            render();
        }

        void handleAudioEvent(app::eAudioEvent &evt)
        {
            std::lock_guard<std::mutex> lock(m_ui_access);
            m_playing = (evt == app::eAudioEvent::PLAYING);
            render();
        }
    };
}