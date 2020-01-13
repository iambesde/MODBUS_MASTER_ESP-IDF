/* FreeModbus Slave Example ESP32

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include "esp_err.h"
#include "sdkconfig.h"
#include "mbcontroller.h"       // for mbcontroller defines and api
#include "deviceparams.h"       // for device parameters structures
#include "esp_log.h"            // for log_write
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "esp_system.h"
#include "esp_event_loop.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "lwip/err.h"
#include "lwip/sys.h"

#define MB_PORT_NUM     (2)           // Number of UART port used for Modbus connection
#define MB_DEV_ADDR     (1)           // The address of device in Modbus network
#define MB_DEV_SPEED    (115200)      // The communication speed of the UART

// Defines below are used to define register start address for each type of Modbus registers
#define MB_REG_DISCRETE_INPUT_START         (0x0000)
#define MB_REG_INPUT_START                  (0x0000)
#define MB_REG_HOLDING_START                (0x0000)
#define MB_REG_COILS_START                  (0x0000)

#define MB_PAR_INFO_GET_TOUT                (10) // Timeout for get parameter info
#define MB_CHAN_DATA_MAX_VAL                (10)
#define MB_CHAN_DATA_OFFSET                 (0.01f)
#define EXAMPLE_ESP_WIFI_SSID      "Intellexus_IOT"
#define EXAMPLE_ESP_WIFI_PASS      "Amvisa@123A"
#define EXAMPLE_ESP_MAXIMUM_RETRY  10

static const char *TAG = "MODBUS_SLAVE_APP";
/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;

/* The event group allows multiple bits for each event, but we only care about one event
 * - are we connected to the AP with an IP? */
const int WIFI_CONNECTED_BIT = BIT0;

static int s_retry_num = 0;

static esp_err_t event_handler(void *ctx, system_event_t *event)
{
    switch(event->event_id) {
    case SYSTEM_EVENT_STA_START:
        esp_wifi_connect();
        break;
    case SYSTEM_EVENT_STA_GOT_IP:
        ESP_LOGI(TAG, "got ip:%s",ip4addr_ntoa(&event->event_info.got_ip.ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
        {
                esp_wifi_connect();
            if (s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY) {
                xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
                s_retry_num++;
                ESP_LOGI(TAG,"retry to connect to the AP");
            }
            ESP_LOGI(TAG,"connect to the AP fail\n");
            break;
        }
    default:
        break;
    }
    return ESP_OK;
}


// An example application of Modbus slave. It is based on freemodbus stack.
// See deviceparams.h file for more information about assigned Modbus parameters.
// These parameters can be accessed from main application and also can be changed
// by external Modbus master host.
void
vlwIPInit( void )
{
	s_wifi_event_group = xEventGroupCreate();

	    tcpip_adapter_init();
	    ESP_ERROR_CHECK(esp_event_loop_init(event_handler, NULL) );

	    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
	    wifi_config_t wifi_config = {
	        .sta = {
	            .ssid = EXAMPLE_ESP_WIFI_SSID,
	            .password = EXAMPLE_ESP_WIFI_PASS
	        },
	    };

	    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
	    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
	    ESP_ERROR_CHECK(esp_wifi_start() );

	    ESP_LOGI(TAG, "wifi_init_sta finished.");
	    ESP_LOGI(TAG, "connect to ap SSID:%s password:%s",
	             EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);

}

void app_main()
{
    mb_communication_info_t comm_info; // Modbus communication parameters

    // Set UART log level
    esp_log_level_set(TAG, ESP_LOG_INFO);
    eMBErrorCode eStatus;
        // initialize NVS
    ESP_ERROR_CHECK(nvs_flash_init());
        // initialize the tcp stack
    tcpip_adapter_init();
    mbcontroller_init(); // Initialization of Modbus controller

    ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");

    vlwIPInit();
    // Setup communication parameters and start stack
    comm_info.mode = MB_MODE_RTU;
    comm_info.slave_addr = MB_DEV_ADDR;
    comm_info.port = MB_PORT_NUM;
    comm_info.baudrate = MB_DEV_SPEED;
    comm_info.parity = MB_PARITY_NONE;
    ESP_ERROR_CHECK(mbcontroller_setup(comm_info));

    ESP_ERROR_CHECK(mbcontroller_start());

    // Destroy of Modbus controller once get maximum value of data_chan0
    printf("Modbus controller destroyed.");
    ESP_ERROR_CHECK(mbcontroller_destroy());
}
