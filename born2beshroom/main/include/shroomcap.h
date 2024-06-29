#ifndef SHROOMCAP_H
# define SHROOMCAP_H

#include "esp_log.h"
#include "esp_err.h"

# define WIFI_SSID		CONFIG_ESP_WIFI_SSID
# define WIFI_PASS		CONFIG_ESP_WIFI_PASSWORD
# define WIFI_FAILURE	1 << 1
# define WIFI_SUCCESS	1 << 0
# define TCP_FAILURE	1 << 1
# define TCP_SUCCESS	1 << 0
# define MAX_FAILURES	CONFIG_ESP_MAXIMUM_RETRY

static char const		*TAG = "SHROOM";

esp_err_t	connect_wifi();
esp_err_t	start_tcp_server();

#endif // SHROOMCAP_H
