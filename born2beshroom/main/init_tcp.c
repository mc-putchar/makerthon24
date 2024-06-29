#include "shroomcap.h"

#include "lwip/sockets.h"
#include "lwip/netdb.h"

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static void	reply_404_error(int fd)
{
	char const *const	reply = "HTTP/1.1 404 File Not Found\r\nConnection: close\r\nContent-Type: text/html\r\nContent-Length: 175\r\n\r\n<html><body><h1>Error 404</h1><hr><h2>Requested page does not exist</h2></body></html>";
	size_t const		size = strlen(reply);
	size_t				pend_bytes;

	pend_bytes = size;
	while (pend_bytes)
	{
		ssize_t s = send(fd, reply + size - pend_bytes, pend_bytes, 0);
		if (s < 1)
		{
			close (fd);
			return;
		}
		pend_bytes -= s;
	}
	close(fd);
}

static void	send_reply(int client_fd, char *url)
{
	char		head[256];
	char		buffer[256];
	char		filename[64];
	size_t		pend_bytes;
	size_t		read_bytes;
	struct stat	st;
	size_t		file_size;
	int			file_fd;

	sprintf(filename, "/spiffs/%s", url);
	if (stat(filename, &st))
	{
		reply_404_error(client_fd);
		return;
	}
	file_size = st.st_size;
	file_fd = open(filename, O_RDONLY);
	if (file_fd == -1)
	{
		reply_404_error(client_fd);
		return;
	}

	sprintf(head, "HTTP/1.1 200 OK\r\nConnection: close\r\nContent-Type: text/html\r\nContent-Length: %zu\r\n\r\n", file_size);
	pend_bytes = strlen(head);
	while (pend_bytes)
	{
		ssize_t s = send(client_fd, head, pend_bytes, 0);
		switch (s)
		{
			case (-1):
				pend_bytes = 0;
				break;
			default:
				pend_bytes -= s;
				break;
		}
	}
	if (file_fd == -1)
		return;
	read_bytes = read(file_fd, buffer, sizeof(buffer) - 1);
	while (read_bytes > 0)
	{
		pend_bytes = read_bytes;
		while (pend_bytes)
		{
			ssize_t s = send(client_fd, buffer + read_bytes - pend_bytes, pend_bytes, 0);
			switch (s)
			{
				case (-1):
					pend_bytes = 0;
					break;
				default:
					pend_bytes -= s;
					break;
			}
		}
		read_bytes = read(file_fd, buffer, sizeof(buffer) - 1);
	}
}

static void send_sensor_data(int fd)
{
	// TODO return latest sensor data
}

esp_err_t	start_tcp_server()
{
	struct addrinfo	*servinfo;
	struct addrinfo	*ptr;
	struct addrinfo	hints = {};
	char const		*node = "0.0.0.0";
	char const		*service = "80";
	int				sock;
	char			read_buff[1024] = {0};

	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	int ret = getaddrinfo(node, service, &hints, &servinfo);
	if (ret) {
		ESP_LOGE(TAG, "Failed getaddrinfo");
		return TCP_FAILURE;
	}
	for (ptr = servinfo; ptr != NULL; ptr = ptr->ai_next) {
		sock = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
		if (sock < 0)
			continue;
		if (bind(sock, ptr->ai_addr, ptr->ai_addrlen) == -1)
			close(sock);
		break;
	}
	freeaddrinfo(servinfo);
	if (!ptr) {
		ESP_LOGE(TAG, "Failed to initialize server socket");
		return TCP_FAILURE;
	}
	if (listen(sock, 16) < 0) {
		ESP_LOGE(TAG, "Failed to listen on bound socket");
		close(sock);
		return TCP_FAILURE;
	}
	ESP_LOGI(TAG, "Started TCP server at port: %s", service);
	bzero(read_buff, sizeof(read_buff));

	while (1) {
		int client = accept(sock, NULL, NULL);
		if (client < 0)	continue;
		ESP_LOGI(TAG, "Client connected");
		ssize_t r = recv(client, read_buff, sizeof(read_buff) - 1, 0);
		switch (r) {
			case -1:
				close(client);
				continue;
			case 0:
				close(client);
				continue;
			default:
				read_buff[r] = 0;
				// ESP_LOGI(TAG, "Received: %s", read_buff);
		}
		if (!strncmp("GET ", read_buff, 4))
		{
			if (!strncmp("/ ", read_buff + 4, 2))
				send_reply(client, "index.html");
			else if (!strncmp("/sensor_data ", read_buff + 4, 13))
				send_sensor_data(client);
			else
			{
				int idx = 4;
				while (read_buff[idx] && read_buff[idx] != ' ')
					++idx;
				if (!read_buff[idx])
				{
					reply_404_error(client);
					continue;
				}
				read_buff[idx] = 0;
				send_reply(client, read_buff + 5);
			}
		}
		close(client);
	}
	return TCP_SUCCESS;
}
