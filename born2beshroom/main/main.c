#include "shroomcap.h"

#include "esp_err.h"
#include "esp_spiffs.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "portmacro.h"

#include "DHT22.h"

QueueHandle_t	g_sensor_que;

static void DHT_reader_task(void *pvParameter)
{
	while(1) {
		t_sensor_data	sensor_data = {};
		int ret = readDHT();
		errorHandler(ret);
		// ESP_LOGI(TAG, "Humidity: %.2f %%	| Temperature: %.2f degC", getHumidity(), getTemperature());
		sensor_data.humidity = getHumidity();
		sensor_data.temperature = getTemperature();
		xQueueOverwrite(g_sensor_que, &sensor_data);
		vTaskDelay(3000 / portTICK_PERIOD_MS);
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

	g_sensor_que = xQueueCreate(1, sizeof(t_sensor_data));
	if (!g_sensor_que)
	{
		ESP_LOGE(TAG, "Failed to create data queue. Terminating");
		return;
	}
	t_sensor_data	sensor_init = {.temperature = 0.0f, .humidity = 0.0f};
	xQueueSend(g_sensor_que, &sensor_init, portMAX_DELAY);
	xQueuePeek(g_sensor_que, &sensor_init, 0);
	ESP_LOGI(TAG, "Peeked: %.2f / %.2f", sensor_init.temperature, sensor_init.humidity);

	setDHTgpio(DHT_GPIO_PIN);
	vTaskDelay(2000 / portTICK_PERIOD_MS);
	xTaskCreate(&DHT_reader_task, "DHT_reader_task", 2048, NULL, 5, NULL);

	// status = init_wifi();
	// if (status != WIFI_SUCCESS)
	// {
	// 	ESP_LOGE(TAG, "Failed to associate with Access Point. Terminating");
	// 	return;
	// }

	status = init_wifi_ap();
	if (status != WIFI_SUCCESS)
	{
		ESP_LOGE(TAG, "Failed to setup Access Point. Terminating");
		return;
	}

	// status = start_tcp_server();
	status = start_server();
	if (status != ESP_OK)
	{
		esp_vfs_spiffs_unregister(NULL);
		ESP_LOGE(TAG, "Failed to connect to remote server. Terminating");
		return;
	}
}