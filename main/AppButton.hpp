#pragma once
#include <cstdint>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "Config.hpp"

namespace app
{
    enum class eBtnEvent
    {
        L_PRESSED,
        L_RELEASED,
        M_CLICK,
        M_DCLICK,
        R_PRESSED,
        R_RELEASED
    };

    class AppButton
    {
    private:
        QueueHandle_t m_event_queue = NULL;

        void send(eBtnEvent evt)
        {
            xQueueSend(m_event_queue, &evt, 0);
        }

        static void pollTask(void *param)
        {
            AppButton &self = *(reinterpret_cast<AppButton *>(param));
            const int DEBOUNCE_TICKS = 3;   // 3 x 10 ms
            const int DCLICK_TICKS = 35;    // 350 ms

            bool left = false, mid = false, right = false;
            int left_cnt = 0, mid_cnt = 0, right_cnt = 0;
            int mid_clicks = 0;
            int mid_wait = 0;

            // returns 1 on a new press, -1 on a release, 0 when nothing changed
            auto check = [&](gpio_num_t pin, bool &state, int &cnt) -> int
            {
                bool raw = (gpio_get_level(pin) == 0);
                if (raw != state)
                {
                    if (++cnt >= DEBOUNCE_TICKS)
                    {
                        state = raw;
                        cnt = 0;
                        return raw ? 1 : -1;
                    }
                }
                else
                {
                    cnt = 0;
                }
                return 0;
            };

            while (true)
            {
                int l = check(cfg::BTN_LEFT, left, left_cnt);
                if (l == 1)
                {
                    self.send(eBtnEvent::L_PRESSED);
                }
                else if (l == -1)
                {
                    self.send(eBtnEvent::L_RELEASED);
                }

                int r = check(cfg::BTN_RIGHT, right, right_cnt);
                if (r == 1)
                {
                    self.send(eBtnEvent::R_PRESSED);
                }
                else if (r == -1)
                {
                    self.send(eBtnEvent::R_RELEASED);
                }

                int m = check(cfg::BTN_MID, mid, mid_cnt);
                if (m == -1)
                {
                    mid_clicks++;
                    mid_wait = 0;
                    if (mid_clicks == 2)
                    {
                        self.send(eBtnEvent::M_DCLICK);
                        mid_clicks = 0;
                    }
                }
                if (mid_clicks == 1 && !mid)
                {
                    if (++mid_wait >= DCLICK_TICKS)
                    {
                        self.send(eBtnEvent::M_CLICK);
                        mid_clicks = 0;
                    }
                }

                vTaskDelay(pdMS_TO_TICKS(10));
            }
        }

    public:
        void init(void)
        {
            m_event_queue = xQueueCreate(10, sizeof(app::eBtnEvent));

            gpio_config_t io = {};
            io.pin_bit_mask = (1ULL << static_cast<int>(cfg::BTN_LEFT)) |
                              (1ULL << static_cast<int>(cfg::BTN_MID)) |
                              (1ULL << static_cast<int>(cfg::BTN_RIGHT));
            io.mode = GPIO_MODE_INPUT;
            io.pull_up_en = GPIO_PULLUP_ENABLE;
            io.pull_down_en = GPIO_PULLDOWN_DISABLE;
            io.intr_type = GPIO_INTR_DISABLE;
            gpio_config(&io);

            xTaskCreate(pollTask, "btn_poll", 3 * 1024, this, 3, nullptr);
        }

        QueueHandle_t getEventQueue(void) const { return m_event_queue; }
    };
}