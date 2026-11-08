#include "esp_log.h"
#include "esp_spiffs.h"
#include "AppUi.hpp"
#include "AppButton.hpp"
#include "AppAudio.hpp"

namespace
{
    app::AppButton m_app_btn;
    app::AppAudio m_app_audio;
    app::AppUi m_app_ui;
}

extern "C" void app_main(void)
{
    esp_vfs_spiffs_conf_t conf = {};
    conf.base_path = "/spiffs";
    conf.partition_label = "storage";
    conf.max_files = 10;
    conf.format_if_mount_failed = false;
    ESP_ERROR_CHECK(esp_vfs_spiffs_register(&conf));

    m_app_btn.init();
    m_app_audio.init();
    m_app_ui.init(&m_app_btn, &m_app_audio);
}