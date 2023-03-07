/*	TCP Client

	This example code is in the Public Domain (or CC0 licensed, at your option.)

	Unless required by applicable law or agreed to in writing, this
	software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
	CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/message_buffer.h"
#include "esp_system.h"
#include "esp_log.h"
#include "lwip/sockets.h"

#include <netdb.h> //hostent

#include "parameter.h"

static const char *TAG = "TCP";

extern MessageBufferHandle_t xMessageBufferTcpToMain;

void tcp_client_task(void *pvParameters)
{
	PARAMETER_t *task_parameter = pvParameters;
	PARAMETER_t param;
	memcpy((char *)&param, task_parameter, sizeof(PARAMETER_t));
	ESP_LOGI(TAG, "Start:param.port=%d param.ipv4=[%s]", param.port, param.ipv4);

	int addr_family = 0;
	int ip_protocol = 0;

	struct sockaddr_in dest_addr;
	memset(&dest_addr, 0, sizeof(dest_addr));
	dest_addr.sin_family = AF_INET;
	dest_addr.sin_port = htons(param.port);
	dest_addr.sin_addr.s_addr = inet_addr(param.ipv4);
	ESP_LOGI(TAG, "dest_addr.sin_addr.s_addr=0x%"PRIx32, dest_addr.sin_addr.s_addr);
	if (dest_addr.sin_addr.s_addr == 0xffffffff) {
		ESP_LOGI(TAG, "convert from host to ip");
		struct hostent *hp;
		hp = gethostbyname(param.ipv4);
		if (hp == NULL) {
			ESP_LOGE(TAG, "gethostbyname fail");
			while(1) { vTaskDelay(1); }
		}
		struct ip4_addr *ip4_addr;
		ip4_addr = (struct ip4_addr *)hp->h_addr;
		dest_addr.sin_addr.s_addr = ip4_addr->addr;
	}
	ESP_LOGI(TAG, "dest_addr.sin_addr.s_addr=0x%"PRIx32, dest_addr.sin_addr.s_addr);

	addr_family = AF_INET;
	ip_protocol = IPPROTO_IP;
	int sock =	socket(addr_family, SOCK_STREAM, ip_protocol);
	if (sock < 0) {
		ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
		vTaskDelete(NULL);
	}
	ESP_LOGI(TAG, "Socket created, connecting to %s:%d", param.ipv4, param.port);

	int err = connect(sock, (struct sockaddr *)&dest_addr, sizeof(struct sockaddr_in6));
	if (err == 0) {
		ESP_LOGI(TAG, "Successfully connected");
	} else {
		ESP_LOGE(TAG, "Socket unable to connect: errno %d", errno);
		vTaskDelete(NULL);
	}

	char rx_buffer[512];
	while (1) {
		int rx_len = recv(sock, rx_buffer, sizeof(rx_buffer) - 1, 0);
		ESP_LOGD(TAG, "rx_len=%d", rx_len);
		if (rx_len <= 0) break;
		rx_buffer[rx_len] = 0; // Null-terminate whatever we received and treat like a string
		ESP_LOGD(TAG, "Received %d bytes from [%s]", rx_len, param.ipv4);
		ESP_LOGD(TAG, "[%.*s]", rx_len, rx_buffer);
		size_t xBytesSent = xMessageBufferSend(xMessageBufferTcpToMain, &rx_buffer, rx_len, 1000/portTICK_PERIOD_MS);
		if( xBytesSent != rx_len ) {
			ESP_LOGE(TAG, "xMessageBufferSend fail");
		}
	} // end while

	ESP_LOGW(TAG, "Shutting down socket from server");
	ESP_LOGW(TAG, "Application terminated");
	shutdown(sock, 0);
	close(sock);
	vTaskDelete(NULL);
}
