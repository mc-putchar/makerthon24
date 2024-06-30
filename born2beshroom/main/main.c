#include "shroomcap.h"

#include "esp_err.h"
#include "esp_spiffs.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "portmacro.h"
// #include "driver/adc.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"

#include "DHT22.h"
#include <stdbool.h>

QueueHandle_t	g_sensor_que;

static adc_oneshot_unit_handle_t adc1_handle;
static adc_cali_handle_t cali_handle = NULL;
static bool do_calibration;

static bool adc_calibration_init(adc_cali_handle_t *out_handle)
{
	adc_cali_handle_t handle = NULL;
	esp_err_t ret;
	bool calibrated = false;

	adc_cali_line_fitting_config_t cali_config = {
		.unit_id = ADC_UNIT_1,
		.atten = ADC_ATTEN_DB_12,
		.bitwidth = ADC_BITWIDTH_DEFAULT,
	};

	ret = adc_cali_create_scheme_line_fitting(&cali_config, &handle);

	if (ret == ESP_OK) {
		*out_handle = handle;
		calibrated = true;
	}

	return calibrated;
}

int	get_moisture_reading(bool do_calibration)
{
	int adc_reading = 0;
	int adc_sum = 0;

	for (int i = 0; i < NO_OF_SAMPLES; i++) {
		ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, SOIL_ADC_CHANNEL, &adc_reading));
		adc_sum += adc_reading;
	}
	adc_reading = adc_sum / NO_OF_SAMPLES;

	int voltage = 0;
	if (do_calibration) {
		ESP_ERROR_CHECK(adc_cali_raw_to_voltage(cali_handle, adc_reading, &voltage));
	} else {
		adc_cali_raw_to_voltage(cali_handle, adc_reading, &voltage);
	}

	ESP_LOGI(TAG, "Raw: %d\tVoltage: %dmV", adc_reading, voltage);
	
	int moisture_percentage = 100 * (DRY_SOIL_VOLTAGE - voltage) / (DRY_SOIL_VOLTAGE - WET_SOIL_VOLTAGE);
	if (moisture_percentage < 0)
		moisture_percentage = 0;
	else if (moisture_percentage > 100)
		moisture_percentage = 100;
	return moisture_percentage;
}

void	init_moisture_sensor(void)
{
	adc_oneshot_unit_init_cfg_t init_config1 = {
		.unit_id = ADC_UNIT_1,
	};
	adc_oneshot_chan_cfg_t config = {
		.bitwidth = ADC_BITWIDTH_DEFAULT,
		.atten = ADC_ATTEN_DB_12,
	};

	ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &adc1_handle));
	ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, SOIL_ADC_CHANNEL, &config));

	do_calibration = adc_calibration_init(&cali_handle);
}

static void DHT_reader_task(void *pvParameter)
{
	while(1) {
		t_sensor_data	sensor_data = {};
		int ret = readDHT();
		errorHandler(ret);
		// ESP_LOGI(TAG, "Humidity: %.2f %%	| Temperature: %.2f degC", getHumidity(), getTemperature());
		sensor_data.humidity = getHumidity();
		sensor_data.temperature = getTemperature();
		sensor_data.soil_moisture = get_moisture_reading(do_calibration);
		xQueueOverwrite(g_sensor_que, &sensor_data);
		vTaskDelay(SENSOR_POLLING_DELAY / portTICK_PERIOD_MS);
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
		.base_path = FILE_PATH_PREFIX,
		.partition_label = NULL,
		.max_files = 5,
		.format_if_mount_failed = true,
	};
	ESP_ERROR_CHECK(esp_vfs_spiffs_register(&spiff_config));

	init_moisture_sensor();

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
	vTaskDelay(SENSOR_POLLING_DELAY / portTICK_PERIOD_MS);
	xTaskCreate(&DHT_reader_task, "DHT_reader_task", 2048, NULL, 5, NULL);

	if (WIFI_MODE_AP)
	{
		status = init_wifi_ap();
		if (status != WIFI_SUCCESS)
		{
			ESP_LOGE(TAG, "Failed to setup Access Point. Terminating");
			return;
		}
	}
	else
	{
		status = init_wifi();
		if (status != WIFI_SUCCESS)
		{
			ESP_LOGE(TAG, "Failed to associate with Access Point. Terminating");
			return;
		}
	}
	status = start_server();
	if (status != ESP_OK)
	{
		esp_vfs_spiffs_unregister(NULL);
		ESP_LOGE(TAG, "Failed to connect to remote server. Terminating");
		return;
	}
}