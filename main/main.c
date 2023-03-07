/*
 * SPDX-FileCopyrightText: 2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/message_buffer.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_log.h"
#if CONFIG_USB_MODE || CONFIG_BOTH_MODE
#include "tinyusb.h"
#include "tusb_cdc_acm.h"
#endif
#include "driver/uart.h"

#include "parameter.h"

// MessageBuffer for receiving data from tcp client
MessageBufferHandle_t xMessageBufferTcpToMain;
// MessageBuffer for for sending data to uart
MessageBufferHandle_t xMessageBufferMainToUart;
// The total number of bytes (not messages) the message buffer will be able to hold at any one time.
size_t xBufferSizeBytes = 1024;

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1

static const char *TAG = "MAIN";

static int s_retry_num = 0;

#if CONFIG_USB_MODE || CONFIG_BOTH_MODE
int line_status = false;
tinyusb_cdcacm_itf_t itf;
#endif

static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
	if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
		esp_wifi_connect();
	} else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
		if (s_retry_num < CONFIG_ESP_MAXIMUM_RETRY) {
			esp_wifi_connect();
			s_retry_num++;
			ESP_LOGI(TAG, "retry to connect to the AP");
		} else {
			xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
		}
		ESP_LOGI(TAG,"connect to the AP fail");
	} else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
		ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
		ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
		s_retry_num = 0;
		xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
	}
}


esp_err_t wifi_init_sta(void)
{
	s_wifi_event_group = xEventGroupCreate();

	ESP_ERROR_CHECK(esp_netif_init());

	ESP_ERROR_CHECK(esp_event_loop_create_default());
	esp_netif_create_default_wifi_sta();

	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_wifi_init(&cfg));

	esp_event_handler_instance_t instance_any_id;
	esp_event_handler_instance_t instance_got_ip;
	ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
		ESP_EVENT_ANY_ID,
		&event_handler,
		NULL,
		&instance_any_id));
	ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
		IP_EVENT_STA_GOT_IP,
		&event_handler,
		NULL,
		&instance_got_ip));

	wifi_config_t wifi_config = {
		.sta = {
			.ssid = CONFIG_ESP_WIFI_SSID,
			.password = CONFIG_ESP_WIFI_PASSWORD,
			/* Setting a password implies station will connect to all security modes including WEP/WPA.
			 * However these modes are deprecated and not advisable to be used. Incase your Access point
			 * doesn't support WPA2, these mode can be enabled by commenting below line */
			.threshold.authmode = WIFI_AUTH_WPA2_PSK,

			.pmf_cfg = {
				.capable = true,
				.required = false
			},
		},
	};
	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
	ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
	//ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
	ESP_ERROR_CHECK(esp_wifi_start() );

	ESP_LOGI(TAG, "wifi_init_sta finished.");

	/* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
	 * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
	esp_err_t ret_value = ESP_OK;
	EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
		WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
		pdFALSE,
		pdFALSE,
		portMAX_DELAY);

	/* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
	 * happened. */
	if (bits & WIFI_CONNECTED_BIT) {
		ESP_LOGI(TAG, "connected to ap SSID:%s password:%s", CONFIG_ESP_WIFI_SSID, CONFIG_ESP_WIFI_PASSWORD);
	} else if (bits & WIFI_FAIL_BIT) {
		ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s", CONFIG_ESP_WIFI_SSID, CONFIG_ESP_WIFI_PASSWORD);
		ret_value = ESP_FAIL;
	} else {
		ESP_LOGE(TAG, "UNEXPECTED EVENT");
		ret_value = ESP_FAIL;
	}

	/* The event will not be processed after unregister */
	ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, instance_got_ip));
	ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, instance_any_id));
	vEventGroupDelete(s_wifi_event_group);
	return ret_value;
}

#if CONFIG_USB_MODE || CONFIG_BOTH_MODE
void tinyusb_cdc_rx_callback(int itf, cdcacm_event_t *event)
{
	/* initialization */
	size_t rx_size = 0;
	ESP_LOGI(TAG, "CONFIG_TINYUSB_CDC_RX_BUFSIZE=%d", CONFIG_TINYUSB_CDC_RX_BUFSIZE);
	uint8_t buf[CONFIG_TINYUSB_CDC_RX_BUFSIZE + 1];

	/* read */
	esp_err_t ret = tinyusb_cdcacm_read(itf, buf, CONFIG_TINYUSB_CDC_RX_BUFSIZE, &rx_size);
	if (ret == ESP_OK) {
		ESP_LOGI(TAG, "Data from channel=%d rx_size=%d", itf, rx_size);
		ESP_LOG_BUFFER_HEXDUMP(TAG, buf, rx_size, ESP_LOG_INFO);
	} else {
		ESP_LOGE(TAG, "Read error");
	}
}

void tinyusb_cdc_line_state_changed_callback(int itf, cdcacm_event_t *event)
{
	int dtr = event->line_state_changed_data.dtr;
	int rts = event->line_state_changed_data.rts;
	ESP_LOGI(TAG, "Line state changed on channel %d: DTR:%d, RTS:%d", itf, dtr, rts);
	if (dtr == 1 && rts == 1) {
		ESP_LOGW("USB", "Connected");
		line_status = true;
	} else {
		ESP_LOGW("USB", "Releaseed");
		line_status = false;
	}
}
#endif // USB_MODE

#if CONFIG_UART_MODE || CONFIG_BOTH_MODE

#define RX_BUF_SIZE 128

void uart_init(void) {
	const uart_config_t uart_config = {
		//.baud_rate = 115200,
		.baud_rate = CONFIG_UART_BAUD_RATE,
		.data_bits = UART_DATA_8_BITS,
		.parity = UART_PARITY_DISABLE,
		.stop_bits = UART_STOP_BITS_1,
		.flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
		.source_clk = UART_SCLK_DEFAULT,
#else
		.source_clk = UART_SCLK_APB,
#endif
	};
	// We won't use a buffer for sending data.
	uart_driver_install(UART_NUM_1, RX_BUF_SIZE * 2, 0, 0, NULL, 0);
	uart_param_config(UART_NUM_1, &uart_config);
	//uart_set_pin(UART_NUM_1, TXD_PIN, RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
	uart_set_pin(UART_NUM_1, CONFIG_UART_TX_GPIO, CONFIG_UART_RX_GPIO, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
}

static void uart_tx_task(void* pvParameters)
{
	ESP_LOGI(pcTaskGetName(NULL), "Use GPIO%d for transmission", CONFIG_UART_TX_GPIO);

	char buffer[512];
	while(1) {
		size_t readBytes = xMessageBufferReceive(xMessageBufferMainToUart, buffer, sizeof(buffer), portMAX_DELAY);
		ESP_LOGD(pcTaskGetName(NULL), "readBytes=%d", readBytes);
		ESP_LOG_BUFFER_HEXDUMP(pcTaskGetName(NULL), buffer, readBytes, ESP_LOG_DEBUG);
		int sendBytes = uart_write_bytes(UART_NUM_1, buffer, readBytes);
		ESP_LOGD(pcTaskGetName(NULL), "sendBytes=%d", sendBytes);
		if (readBytes != sendBytes) {
			ESP_LOGW(pcTaskGetName(NULL), "uart_write_bytes fail. [%d]-->[%d]", readBytes, sendBytes);
		}
	} // end while

	// Never reach here
	vTaskDelete(NULL);
}
#endif // UART_MODE

void tcp_client_task(void *pvParameters);

void app_main(void)
{
	// Initialize NVS
	esp_err_t ret = nvs_flash_init();
	if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
		ESP_ERROR_CHECK(nvs_flash_erase());
		ret = nvs_flash_init();
	}
	ESP_ERROR_CHECK(ret);

	// Initialize WiFi
	wifi_init_sta();

#if CONFIG_USB_MODE || CONFIG_BOTH_MODE
	ESP_LOGI(TAG, "USB initialization");
	const tinyusb_config_t tusb_cfg = {};

	ESP_ERROR_CHECK(tinyusb_driver_install(&tusb_cfg));

	tinyusb_config_cdcacm_t acm_cfg = {
		.usb_dev = TINYUSB_USBDEV_0,
		.cdc_port = TINYUSB_CDC_ACM_0,
		.rx_unread_buf_sz = 64,
		.callback_rx = &tinyusb_cdc_rx_callback, // the first way to register a callback
		.callback_rx_wanted_char = NULL,
		.callback_line_state_changed = NULL,
		.callback_line_coding_changed = NULL
	};

	ESP_ERROR_CHECK(tusb_cdc_acm_init(&acm_cfg));
	/* the second way to register a callback */
	ESP_ERROR_CHECK(tinyusb_cdcacm_register_callback(
		TINYUSB_CDC_ACM_0,
		CDC_EVENT_LINE_STATE_CHANGED,
		&tinyusb_cdc_line_state_changed_callback));

#if (CONFIG_TINYUSB_CDC_COUNT > 1)
	acm_cfg.cdc_port = TINYUSB_CDC_ACM_1;
	ESP_ERROR_CHECK(tusb_cdc_acm_init(&acm_cfg));
	ESP_ERROR_CHECK(tinyusb_cdcacm_register_callback(
		TINYUSB_CDC_ACM_1,
		CDC_EVENT_LINE_STATE_CHANGED,
		&tinyusb_cdc_line_state_changed_callback));
#endif

	ESP_LOGI(TAG, "USB initialization DONE");
#endif // USB_MODE

#if CONFIG_UART_MODE || CONFIG_BOTH_MODE
	/* uart initialize */
	uart_init();
	ESP_LOGI(TAG, "UART initialization DONE");
#endif

	// Create MessageBuffer
	xMessageBufferTcpToMain= xMessageBufferCreate(xBufferSizeBytes);
	configASSERT( xMessageBufferTcpToMain);
	xMessageBufferMainToUart = xMessageBufferCreate(xBufferSizeBytes);
	configASSERT( xMessageBufferMainToUart );

	PARAMETER_t param;
	param.port = CONFIG_EXAMPLE_PORT;
	strcpy(param.ipv4, CONFIG_EXAMPLE_IPV4_ADDR);
	param.taskHandle = xTaskGetCurrentTaskHandle();
	xTaskCreate(tcp_client_task, "tcp_client", 1024*4, (void *)&param, 5, NULL);

#if CONFIG_UART_MODE || CONFIG_BOTH_MODE
	xTaskCreate(uart_tx_task, "UART", 1024*4, NULL, 2, NULL);
#endif

	char buffer[512];
	while(1) {
		size_t readBytes = xMessageBufferReceive(xMessageBufferTcpToMain, buffer, sizeof(buffer), portMAX_DELAY);
		if (readBytes > 0) {
#if CONFIG_USB_MODE || CONFIG_BOTH_MODE
			ESP_LOGI(TAG, "TCP-->USB [%.*s]", readBytes, buffer);
			if (line_status) {
				size_t sendBytes = tinyusb_cdcacm_write_queue(itf, (uint8_t *)buffer, readBytes);
				ESP_LOGD(TAG, "readBytes=%d tinyusb_cdcacm_write_queue sendBytes=%d", readBytes, sendBytes);
				if (readBytes != sendBytes) {
					ESP_LOGW(TAG, "tinyusb_cdcacm_write_queue fail. [%d]-->[%d]", readBytes, sendBytes);
				}
				if (readBytes < 64) tinyusb_cdcacm_write_flush(itf, 0);
			} else {
				ESP_LOGW(TAG, "USB not online");
			}
#endif // USB_MODE

#if CONFIG_UART_MODE || CONFIG_BOTH_MODE
			ESP_LOGI(TAG, "TCP-->UART [%.*s]", readBytes, buffer);
			size_t sendBytes = xMessageBufferSend( xMessageBufferMainToUart, buffer, readBytes, (100/portTICK_PERIOD_MS));
			ESP_LOGD(TAG, "readBytes=%d xMessageBufferSend sendBytes=%d", readBytes, sendBytes);
			if (readBytes != sendBytes) {
				ESP_LOGW(TAG, "xMessageBufferSend fail. [%d]-->[%d]", readBytes, sendBytes);
			}
#endif
		}
	}

}
