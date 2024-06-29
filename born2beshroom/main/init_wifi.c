#include "esp_event.h"
#include "esp_event_base.h"
#include "esp_netif.h"
#include "esp_netif_types.h"
#include "esp_wifi_types_generic.h"
#include "lwip/ip4_addr.h"
#include "shroomcap.h"

#include "esp_wifi.h"
#include "esp_wifi_default.h"

#include <string.h>

static EventGroupHandle_t	wifi_event_group;
static int					s_retry_num = 0;


static void wifi_event_handler_ap(void *arg, esp_event_base_t event_base,
		int32_t event_id, void *event_data)
{
	if (event_id == WIFI_EVENT_AP_STACONNECTED)
	{
		wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *)event_data;
		ESP_LOGI(TAG, "station joined, AID=%d", event->aid);
	}
	else if (event_id == WIFI_EVENT_AP_STADISCONNECTED)
	{
		wifi_event_ap_stadisconnected_t *event = (wifi_event_ap_stadisconnected_t *)event_data;
		ESP_LOGI(TAG, "station left, AID=%d", event->aid);
	}
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
		int32_t event_id, void *event_data)
{
	if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
	{
		ESP_LOGI(TAG, "Connecting to Access Point...");
		esp_wifi_connect();
	}
	else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
	{
		if (s_retry_num < MAX_FAILURES)
		{
			ESP_LOGI(TAG, "Reconnecting...");
			esp_wifi_connect();
			++s_retry_num;
		}
		else
		{
			xEventGroupSetBits(wifi_event_group, WIFI_FAILURE);
		}
	}
}

static void	ip_event_handler(void *arg, esp_event_base_t event_base,
		int32_t event_id, void *event_data)
{
	if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
	{
		ip_event_got_ip_t	*event = (ip_event_got_ip_t *)event_data;
		ESP_LOGI(TAG, "STA IP: " IPSTR, IP2STR(&event->ip_info.ip));
		s_retry_num = 0;
		xEventGroupSetBits(wifi_event_group, WIFI_SUCCESS);
	}
}

esp_err_t	init_wifi_ap()
{
	ESP_ERROR_CHECK(esp_netif_init());
	ESP_ERROR_CHECK(esp_event_loop_create_default());

	esp_netif_t	*p_netif = esp_netif_create_default_wifi_ap();
	ESP_ERROR_CHECK(esp_event_handler_instance_register(
		WIFI_EVENT,
		ESP_EVENT_ANY_ID,
		&wifi_event_handler_ap,
		NULL,
		NULL));

	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_wifi_init(&cfg));

	wifi_config_t	wifi_config = {
		.ap = {
			.ssid = WIFI_SSID,
			.ssid_len = strlen(WIFI_SSID),
			.channel = WIFI_CHANNEL,
			.password = WIFI_PASS,
			.max_connection = MAX_STA_CONN,
			.authmode = WIFI_AUTH_WPA2_WPA3_PSK
		},
	};
	if (!strlen(WIFI_PASS))
		wifi_config.ap.authmode = WIFI_AUTH_OPEN;

	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
	ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
	ESP_ERROR_CHECK(esp_wifi_start());

	esp_netif_ip_info_t	ip_info;
	IP4_ADDR(&ip_info.ip, 192, 168, 1, 1);
	IP4_ADDR(&ip_info.gw, 192, 168, 1, 1);
	IP4_ADDR(&ip_info.netmask, 255, 255, 255, 0);
	esp_netif_dhcps_stop(p_netif);
	esp_netif_set_ip_info(p_netif, &ip_info);
	esp_netif_dhcps_start(p_netif);
	ESP_LOGI(TAG, "Server available at 192.168.1.1");
	return TCP_SUCCESS;
}

esp_err_t	init_wifi()
{
	int	status = WIFI_FAILURE;
	ESP_ERROR_CHECK(esp_netif_init());
	ESP_ERROR_CHECK(esp_event_loop_create_default());
	esp_netif_create_default_wifi_sta();
	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_wifi_init(&cfg));

	wifi_event_group = xEventGroupCreate();
	esp_event_handler_instance_t wifi_handler_event_instance;
	ESP_ERROR_CHECK(esp_event_handler_instance_register(
		WIFI_EVENT,
		ESP_EVENT_ANY_ID,
		&wifi_event_handler,
		NULL,
		&wifi_handler_event_instance));

	esp_event_handler_instance_t got_ip_event_instance;
	ESP_ERROR_CHECK(esp_event_handler_instance_register(
		IP_EVENT,
		IP_EVENT_STA_GOT_IP,
		&ip_event_handler,
		NULL,
		&got_ip_event_instance));

	wifi_config_t	wifi_config = {
		.sta = {
			.ssid = WIFI_SSID,
			.password = WIFI_PASS,
			.threshold.authmode = WIFI_AUTH_WPA2_PSK,
			.pmf_cfg = {
				.capable = true,
				.required = false
			},
		},
	};

	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
	ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
	ESP_ERROR_CHECK(esp_wifi_start());
	ESP_LOGI(TAG, "STA init complete");

	EventBits_t bits = xEventGroupWaitBits(
		wifi_event_group,
		WIFI_SUCCESS | WIFI_FAILURE,
		pdFALSE,
		pdFALSE,
		portMAX_DELAY);

	if (bits & WIFI_SUCCESS)
	{
		ESP_LOGI(TAG, "Connected to Access Point");
		status = WIFI_SUCCESS;
	}
	else if (bits & WIFI_FAILURE)
	{
		ESP_LOGI(TAG, "Failed to connect to Access Point");
		status = WIFI_FAILURE;
	}
	else
	{
		ESP_LOGE(TAG, "UNKOWN EVENT");
		status = WIFI_FAILURE;
	}

	ESP_ERROR_CHECK(esp_event_handler_instance_unregister(
		IP_EVENT,
		IP_EVENT_STA_GOT_IP,
		got_ip_event_instance));
	ESP_ERROR_CHECK(esp_event_handler_instance_unregister(
		WIFI_EVENT,
		ESP_EVENT_ANY_ID,
		wifi_handler_event_instance));
	vEventGroupDelete(wifi_event_group);
	
	return (status);
}
