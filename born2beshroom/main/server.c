#include "esp_err.h"
#include "esp_log.h"
#include "shroomcap.h"

#include "esp_http_server.h"

#include <stdio.h>
#include <time.h>

#define SCRATCH_BUF_SIZE	4096

typedef struct s_serv_data
{
	char	scratch[SCRATCH_BUF_SIZE];
}	t_serv_data;

esp_err_t	get_favicon_handler(httpd_req_t *req)
{
	extern unsigned char const	favicon_ico_start[] asm("_binary_favicon_ico_start");
	extern unsigned char const	favicon_ico_end[] asm("_binary_favicon_ico_end");
	size_t const favicon_ico_size = (favicon_ico_end - favicon_ico_start);
	httpd_resp_set_type(req, "image/x-icon");
	httpd_resp_send(req, (char const *)favicon_ico_start, favicon_ico_size);
	return ESP_OK;
}

esp_err_t	send_file_chunks(httpd_req_t *req, char const *file, char const *type)
{
	char	filepath[128];
	sprintf(filepath, "%s/%s", FILE_PATH_PREFIX, file);
	FILE *fd = fopen(filepath, "r");
	if (!fd)
	{
		ESP_LOGE(TAG, "Failed to open file %s", filepath);
		httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to read existing file");
		return ESP_FAIL;
	}
	httpd_resp_set_type(req, type);
	char *chunk = ((t_serv_data *)req->user_ctx)->scratch;
	size_t	chunksize;
	do
	{
		chunksize = fread(chunk, 1, SCRATCH_BUF_SIZE, fd);
		if (chunksize > 1)
		{
			if (httpd_resp_send(req, chunk, chunksize) != ESP_OK)
			{
				httpd_resp_sendstr_chunk(req, NULL);
				httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to send file");
				return ESP_FAIL;
			}
		}
	} while (chunksize);
	fclose(fd);
	httpd_resp_set_hdr(req, "Connection", "close");
	httpd_resp_send_chunk(req, NULL, 0);
	return ESP_OK;
}

esp_err_t	get_index_handler(httpd_req_t *req)
{
	return send_file_chunks(req, "index.html", "text/html");
}

esp_err_t	get_css_handler(httpd_req_t *req)
{
	return send_file_chunks(req, "shroom.css", "text/css");
}

esp_err_t	get_js_handler(httpd_req_t *req)
{
	return send_file_chunks(req, "get_sensor_data.js", "text/javascript");
}

esp_err_t get_sensor_data(httpd_req_t *req)
{
	char			json[256];
	t_sensor_data	data;

	if (!g_sensor_que)
	{
		ESP_LOGE(TAG, "Queue not initialized");
		return ESP_FAIL;
	}
	xQueuePeek(g_sensor_que, &data, portMAX_DELAY);
	sprintf(json, "{\"temperature\": \"%.2f\", \"humidity\": \"%.2f\", \"soilMoisture\": \"%d\", \"co2Level\": \"Not implemented\"}", data.temperature, data.humidity, data.soil_moisture);
	httpd_resp_set_type(req, HTTPD_TYPE_JSON);
	httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
	return ESP_OK;
}

esp_err_t	start_server(void)
{
	httpd_config_t	config = HTTPD_DEFAULT_CONFIG();
	httpd_handle_t	server = NULL;
	t_serv_data		*ctx = malloc(sizeof(t_serv_data));
	if (!ctx)
	{
		ESP_LOGE(TAG, "Failed to allocate memory for server data");
		return ESP_FAIL;
	}
	if (httpd_start(&server, &config) != ESP_OK)
	{
		ESP_LOGE(TAG, "Failed to start server");
		return ESP_FAIL;
	}
	httpd_uri_t	uri_root = {
		.uri = "/",
		.method = HTTP_GET,
		.handler = get_index_handler,
		.user_ctx = ctx
	};
	httpd_register_uri_handler(server, &uri_root);
	httpd_uri_t	uri_sensor = {
		.uri = "/sensor_data",
		.method = HTTP_GET,
		.handler = get_sensor_data,
		.user_ctx = NULL
	};
	httpd_register_uri_handler(server, &uri_sensor);
	httpd_uri_t	uri_css = {
		.uri = "/shroom.css",
		.method = HTTP_GET,
		.handler = get_css_handler,
		.user_ctx = ctx
	};
	httpd_register_uri_handler(server, &uri_css);
	httpd_uri_t	uri_js = {
		.uri = "/get_sensor_data.js",
		.method = HTTP_GET,
		.handler = get_js_handler,
		.user_ctx = ctx
	};
	httpd_register_uri_handler(server, &uri_js);
	httpd_uri_t	uri_favicon = {
		.uri = "/favicon.ico",
		.method = HTTP_GET,
		.handler = get_favicon_handler,
		.user_ctx = NULL
	};
	httpd_register_uri_handler(server, &uri_favicon);

	return ESP_OK;
}

void	stop_server(httpd_handle_t server)
{
	if (server)
		httpd_stop(server);
}
