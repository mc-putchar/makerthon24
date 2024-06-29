#include "shroomcap.h"

#include "esp_err.h"
#include "esp_spiffs.h"
#include "freertos/idf_additions.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "portmacro.h"

#include "DHT22.h"

static void DHT_reader_task(void *pvParameter)
{
	while(1) {
		int ret = readDHT();
		errorHandler(ret);
		// ESP_LOGI(TAG, "Humidity: %.2f %%	| Temperature: %.2f degC", getHumidity(), getTemperature());
		vTaskDelay(2000 / portTICK_PERIOD_MS);
	}
}

void app_main(void)
{
	esp_err_t	status = WIFI_FAILURE;

	/* Init non volatile flash to store config */
	esp_err_t	ret = nvs_flash_init();
	if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
	{
		ESP_ERROR_CHECK(nvs_flash_erase());
		ret = nvs_flash_init();
	}
	ESP_ERROR_CHECK(ret);

	/* Init SPI Flash file system */
	esp_vfs_spiffs_conf_t spiff_config = {
		.base_path = "/spiffs",
		.partition_label = NULL,
		.max_files = 5,
		.format_if_mount_failed = true,
	};
	ESP_ERROR_CHECK(esp_vfs_spiffs_register(&spiff_config));

	setDHTgpio(27);
	vTaskDelay(2000 / portTICK_PERIOD_MS);
	xTaskCreate(&DHT_reader_task, "DHT_reader_task", 2048, NULL, 5, NULL);

	status = connect_wifi();
	if (status != WIFI_SUCCESS)
	{
		ESP_LOGE(TAG, "Failed to associate with Access Point. Terminating");
		return;
	}

	status = start_tcp_server();
	esp_vfs_spiffs_unregister(NULL);
	if (status != TCP_SUCCESS)
	{
		ESP_LOGE(TAG, "Failed to connect to remote server. Terminating");
		return;
	}
}