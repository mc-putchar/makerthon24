#ifndef SHROOMCAP_H
# define SHROOMCAP_H

#include "esp_err.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "freertos/idf_additions.h"

# define WIFI_SSID			CONFIG_ESP_WIFI_SSID
# define WIFI_PASS			CONFIG_ESP_WIFI_PASSWORD
# define WIFI_CHANNEL		CONFIG_ESP_WIFI_CHANNEL
# define MAX_STA_CONN		CONFIG_ESP_MAX_STA_CONN
# define WIFI_FAILURE		1 << 1
# define WIFI_SUCCESS		1 << 0
# define TCP_FAILURE		1 << 1
# define TCP_SUCCESS		1 << 0
# define MAX_FAILURES		CONFIG_ESP_MAXIMUM_RETRY
# define DHT_GPIO_PIN		27

static char const		*TAG = "SHROOM";
static char const		*FILE_PATH_PREFIX = "/spiffs/";

typedef struct s_sensor_data
{
	float	temperature;
	float	humidity;
}	t_sensor_data;

extern QueueHandle_t	g_sensor_que;

esp_err_t	init_wifi();
esp_err_t	init_wifi_ap();
// esp_err_t	start_tcp_server();
esp_err_t	start_server(void);
void		stop_server(httpd_handle_t server);

#endif // SHROOMCAP_H
