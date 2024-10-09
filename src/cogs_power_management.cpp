#include "esp_sleep.h"
#include "esp_pm.h"
#include "sdkconfig.h"
#include "cogs_util.h"

namespace cogs_pm
{
    void begin()
    {
#ifdef CONFIG_PM_ENABLE
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 1, 4)
        esp_pm_config_t pm;
        esp_pm_get_configuration(&pm);
        esp_pm_get_configuration(&old_pm);
        pm.light_sleep_enable = true;
        pm.max_freq_mhz = 240;
        pm.min_freq_mhz = 80;
        ESP_ERROR_CHECK_WITHOUT_ABORT(esp_pm_configure(&pm));

#elif defined(CONFIG_IDF_TARGET_ESP32S3)
        esp_pm_config_esp32s3_t pm;
        esp_pm_get_configuration(&pm);
        pm.light_sleep_enable = true;
        pm.max_freq_mhz = 240;
        pm.min_freq_mhz = 80;
        ESP_ERROR_CHECK_WITHOUT_ABORT(esp_pm_configure(&pm));

#elif defined(CONFIG_IDF_TARGET_ESP32S2)
        esp_pm_config_esp32s2_t pm;
        esp_pm_get_configuration(&pm);
        pm.light_sleep_enable = true;
        pm.max_freq_mhz = 240;
        pm.min_freq_mhz = 80;
        ESP_ERROR_CHECK_WITHOUT_ABORT(esp_pm_configure(&pm));

#elif defined(ESP32)
        esp_pm_config_esp32_t pm;
        esp_pm_get_configuration(&pm);
        pm.light_sleep_enable = true;
        pm.max_freq_mhz = 240;
        pm.min_freq_mhz = 80;
        ESP_ERROR_CHECK_WITHOUT_ABORT(esp_pm_configure(&pm));
#elif defined(ESP8266)
        esp_pm_config_esp8266_t pm;
        esp_pm_get_configuration(&pm);
        pm.light_sleep_enable = true;
        pm.max_freq_mhz = 240;
        pm.min_freq_mhz = 80;
        ESP_ERROR_CHECK_WITHOUT_ABORT(esp_pm_configure(&pm));
#else
        cogs::logInfo("Power management not supported");

#endif

#endif

#ifndef CONFIG_PM_ENABLE
        cogs::logInfo("Power management not supported or not enabled at SDK level");
#endif
    }
}