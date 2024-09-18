#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include "sdkconfig.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"

#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_system.h"
#include "esp_err.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_websocket_client.h"
#include "usb_stream.h"
#include <cJSON.h>

static const char *TAG = "MAIN";
static const char *WS_Subprotocol = "ESP32";
static const char *WS_URI = "ws://192.168.50.80";
static const int WS_PORT = 8080;

#define ENABLE_UVC_CAMERA_FUNCTION        1        /* enable uvc function */
#define ENABLE_UAC_MIC_SPK_FUNCTION       1        /* enable uac mic+spk function */
#if (ENABLE_UAC_MIC_SPK_FUNCTION)
static uint32_t s_mic_samples_frequence = 0;
static uint32_t s_mic_ch_num = 0;
static uint32_t s_mic_bit_resolution = 0;
#endif

#define DEMO_UVC_FRAME_WIDTH        640
#define DEMO_UVC_FRAME_HEIGHT       480
#define DEMO_UVC_XFER_BUFFER_SIZE (60 * 1024)

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT  BIT0
#define WIFI_FAIL_BIT       BIT1
#define NO_DATA_TIMEOUT_SEC 5

static int s_retry_num = 0;
esp_websocket_client_handle_t client;
static bool enable_send_image = true;
static bool enable_send_audio = false;

static void camera_frame_cb(uvc_frame_t *frame, void *ptr)
{
    // ESP_LOGI(TAG, "uvc callback! frame_format = %d, seq = %"PRIu32", width = %"PRIu32", height = %"PRIu32", length = %u, ptr = %d",
    //          frame->frame_format, frame->sequence, frame->width, frame->height, frame->data_bytes, (int) ptr);

    if(client != NULL && enable_send_image){
        esp_websocket_client_send_bin(client, frame->data, frame->data_bytes, portMAX_DELAY);
    }
}

static void mic_frame_cb(mic_frame_t *frame, void *ptr)
{
    // We should using higher baudrate here, to reduce the blocking time here
    // ESP_LOGI(TAG, "mic callback! bit_resolution = %u, samples_frequence = %"PRIu32", data_bytes = %"PRIu32,
    //          frame->bit_resolution, frame->samples_frequence, frame->data_bytes);
    if(client != NULL && enable_send_audio){
        esp_websocket_client_send_bin(client, frame->data, frame->data_bytes, portMAX_DELAY);
    }
}

static void stream_state_changed_cb(usb_stream_state_t event, void *arg)
{
    switch (event) {
    case STREAM_CONNECTED: {
        size_t frame_size = 0;
        size_t frame_index = 0;
#if (ENABLE_UVC_CAMERA_FUNCTION)
        uvc_frame_size_list_get(NULL, &frame_size, &frame_index);
        if (frame_size) {
            ESP_LOGI(TAG, "UVC: get frame list size = %u, current = %u", frame_size, frame_index);
            uvc_frame_size_t *uvc_frame_list = (uvc_frame_size_t *)malloc(frame_size * sizeof(uvc_frame_size_t));
            uvc_frame_size_list_get(uvc_frame_list, NULL, NULL);
            for (size_t i = 0; i < frame_size; i++) {
                ESP_LOGI(TAG, "\tframe[%u] = %ux%u", i, uvc_frame_list[i].width, uvc_frame_list[i].height);
            }
            free(uvc_frame_list);
        } else {
            ESP_LOGW(TAG, "UVC: get frame list size = %u", frame_size);
        }
#endif
#if (ENABLE_UAC_MIC_SPK_FUNCTION)
        uac_frame_size_list_get(STREAM_UAC_MIC, NULL, &frame_size, &frame_index);
        if (frame_size) {
            ESP_LOGI(TAG, "UAC MIC: get frame list size = %u, current = %u", frame_size, frame_index);
            uac_frame_size_t *mic_frame_list = (uac_frame_size_t *)malloc(frame_size * sizeof(uac_frame_size_t));
            uac_frame_size_list_get(STREAM_UAC_MIC, mic_frame_list, NULL, NULL);
            for (size_t i = 0; i < frame_size; i++) {
                ESP_LOGI(TAG, "\t [%u] ch_num = %u, bit_resolution = %u, samples_frequence = %"PRIu32 ", samples_frequence_min = %"PRIu32 ", samples_frequence_max = %"PRIu32,
                         i, mic_frame_list[i].ch_num, mic_frame_list[i].bit_resolution, mic_frame_list[i].samples_frequence,
                         mic_frame_list[i].samples_frequence_min, mic_frame_list[i].samples_frequence_max);
            }
            s_mic_samples_frequence = mic_frame_list[frame_index].samples_frequence;
            s_mic_ch_num = mic_frame_list[frame_index].ch_num;
            s_mic_bit_resolution = mic_frame_list[frame_index].bit_resolution;
            if (s_mic_ch_num != 1) {
                ESP_LOGW(TAG, "UAC MIC: only support 1 channel in this example");
            }
            ESP_LOGI(TAG, "UAC MIC: use frame[%u] ch_num = %"PRIu32", bit_resolution = %"PRIu32", samples_frequence = %"PRIu32,
                     frame_index, s_mic_ch_num, s_mic_bit_resolution, s_mic_samples_frequence);
            free(mic_frame_list);
        } else {
            ESP_LOGW(TAG, "UAC MIC: get frame list size = %u", frame_size);
        }
#endif
        ESP_LOGI(TAG, "Device connected");
        break;
    }
    case STREAM_DISCONNECTED:
        ESP_LOGI(TAG, "Device disconnected");
        break;
    default:
        ESP_LOGE(TAG, "Unknown event");
        break;
    }
}

static void websocket_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_websocket_event_data_t *data = (esp_websocket_event_data_t *)event_data;
    switch (event_id) {
    case WEBSOCKET_EVENT_CONNECTED:
        ESP_LOGI(TAG, "WEBSOCKET_EVENT_CONNECTED");
        break;
    case WEBSOCKET_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "WEBSOCKET_EVENT_DISCONNECTED");
        break;
    case WEBSOCKET_EVENT_DATA:
        ESP_LOGI(TAG, "WEBSOCKET_EVENT_DATA");
        ESP_LOGI(TAG, "Received opcode=%d", data->op_code);
        if (data->op_code == 0x1) {
            ESP_LOGI(TAG, "Received=%.*s\n\n", data->data_len, (char *)data->data_ptr);
            cJSON *root = cJSON_Parse(data->data_ptr);
            if (root) {
                cJSON *command = cJSON_GetObjectItem(root, "command");
                ESP_LOGI(TAG, "Json={'command': '%s'}", command->valuestring);
                cJSON *enabled = cJSON_GetObjectItem(root, "enabled");
                ESP_LOGI(TAG, "Json={'enabled': '%d'}", enabled->valueint);
                //ESP_LOGI(TAG, "Json={'command': '%s', 'enabled': '%s'}", id->valuestring, name->valuestring);


                if(strcmp(command->valuestring, "toggleAudio") == 0){
                    enable_send_audio = enabled->valueint == 0 ? false : true;

                }else if(strcmp(command->valuestring, "toggleImage") == 0){
                    enable_send_image = enabled->valueint == 0 ? false : true;

                }
                cJSON_Delete(root);


            }

        }

        // // If received data contains json structure it succeed to parse
        // cJSON *root = cJSON_Parse(data->data_ptr);
        // if (root) {
        //     for (int i = 0 ; i < cJSON_GetArraySize(root) ; i++) {
        //         cJSON *elem = cJSON_GetArrayItem(root, i);
        //         cJSON *id = cJSON_GetObjectItem(elem, "id");
        //         cJSON *name = cJSON_GetObjectItem(elem, "name");
        //         ESP_LOGW(TAG, "Json={'id': '%s', 'name': '%s'}", id->valuestring, name->valuestring);
        //     }
        //     cJSON_Delete(root);
        // }

        // ESP_LOGW(TAG, "Total payload length=%d, data_len=%d, current payload offset=%d\r\n", data->payload_len, data->data_len, data->payload_offset);

        //xTimerReset(shutdown_signal_timer, portMAX_DELAY);
        break;
    case WEBSOCKET_EVENT_ERROR:
        ESP_LOGI(TAG, "WEBSOCKET_EVENT_ERROR");
        break;
    }
}

static void websocket_app_start(void)
{
    esp_websocket_client_config_t websocket_cfg = {};
	websocket_cfg.uri = WS_URI;
	websocket_cfg.port = WS_PORT;
    websocket_cfg.subprotocol = WS_Subprotocol;

    ESP_LOGI(TAG, "Connecting to %s...", websocket_cfg.uri);
    client = esp_websocket_client_init(&websocket_cfg);
    esp_websocket_register_events(client, WEBSOCKET_EVENT_ANY, websocket_event_handler, (void *)client);
    esp_websocket_client_start(client);
}

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

void wifi_init_sta()
{
	s_wifi_event_group = xEventGroupCreate();

	ESP_LOGI(TAG,"ESP-IDF esp_netif");
	ESP_ERROR_CHECK(esp_netif_init());
	ESP_ERROR_CHECK(esp_event_loop_create_default());
	esp_netif_t *netif = esp_netif_create_default_wifi_sta();
	assert(netif);



	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_wifi_init(&cfg));

	ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
	ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL));

	wifi_config_t wifi_config = {
		.sta = {
			.ssid = CONFIG_ESP_WIFI_SSID,
			.password = CONFIG_ESP_WIFI_PASSWORD
		},
	};
	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
	ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
	ESP_ERROR_CHECK(esp_wifi_start() );

	ESP_LOGI(TAG, "wifi_init_sta finished.");

	/* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
	 * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
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
	} else {
		ESP_LOGE(TAG, "UNEXPECTED EVENT");
	}
	vEventGroupDelete(s_wifi_event_group);
}

void app_main(void)
{
    // Initialize NVS
	esp_err_t ret = nvs_flash_init();
	if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
		ESP_ERROR_CHECK(nvs_flash_erase());
		ret = nvs_flash_init();
	}
	ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, ">>>WiFi Connect");

    wifi_init_sta();

	ESP_LOGI(TAG, ">>>USB Device Connect");
     /* malloc double buffer for usb payload, xfer_buffer_size >= frame_buffer_size*/
    uint8_t *xfer_buffer_a = (uint8_t *)malloc(DEMO_UVC_XFER_BUFFER_SIZE);
    assert(xfer_buffer_a != NULL);
    uint8_t *xfer_buffer_b = (uint8_t *)malloc(DEMO_UVC_XFER_BUFFER_SIZE);
    assert(xfer_buffer_b != NULL);

    /* malloc frame buffer for a jpeg frame*/
    uint8_t *frame_buffer = (uint8_t *)malloc(DEMO_UVC_XFER_BUFFER_SIZE);
    assert(frame_buffer != NULL);

    uvc_config_t uvc_config = {
        /* match the any resolution of current camera (first frame size as default) */
        .frame_width = DEMO_UVC_FRAME_WIDTH,
        .frame_height = DEMO_UVC_FRAME_HEIGHT,
        .frame_interval = FPS2INTERVAL(15),
        .xfer_buffer_size = DEMO_UVC_XFER_BUFFER_SIZE,
        .xfer_buffer_a = xfer_buffer_a,
        .xfer_buffer_b = xfer_buffer_b,
        .frame_buffer_size = DEMO_UVC_XFER_BUFFER_SIZE,
        .frame_buffer = frame_buffer,
        .frame_cb = &camera_frame_cb,
        .frame_cb_arg = NULL,
    };
    /* config to enable uvc function */
    ret = uvc_streaming_config(&uvc_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "uvc streaming config failed");
    }
        uac_config_t uac_config = {
        .mic_bit_resolution = UAC_BITS_ANY,
        .mic_samples_frequence = UAC_FREQUENCY_ANY,
        .spk_bit_resolution = UAC_BITS_ANY,
        .spk_samples_frequence = UAC_FREQUENCY_ANY,
        .spk_buf_size = 16000,
        .mic_cb = &mic_frame_cb,
        .mic_cb_arg = NULL,
        /* Set flags to suspend speaker, user need call usb_streaming_control
        later to resume the speaker*/
        .flags = FLAG_UAC_SPK_SUSPEND_AFTER_START,
    };
    ret = uac_streaming_config(&uac_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "uac streaming config failed");
    }

    /* register the state callback to get connect/disconnect event
    * in the callback, we can get the frame list of current device
    */
    ESP_ERROR_CHECK(usb_streaming_state_register(&stream_state_changed_cb, NULL));
    /* start usb streaming, UVC and UAC MIC will start streaming because SUSPEND_AFTER_START flags not set */
    ESP_ERROR_CHECK(usb_streaming_start());
    ESP_ERROR_CHECK(usb_streaming_connect_wait(portMAX_DELAY));

	ESP_LOGI(TAG, ">>>Websocket APP");
    websocket_app_start();
    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("httpd_txrx", ESP_LOG_INFO);
}