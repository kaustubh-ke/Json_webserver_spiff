/*
default name: "ESP32 Test"
password: "hello world"

go to 192.168.4.1 in a browser

*/

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "lwip/api.h"

#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_event_loop.h"
#include "nvs_flash.h"
#include "driver/ledc.h"

#include "string.h"

#include "websocket_server.h"

#include "cJSON.h"
#include <stdio.h>
#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include "esp_err.h"
#include "sdkconfig.h"
#include <errno.h>
#include <sys/fcntl.h>
#include <unistd.h>
#include <ctype.h>
#include "esp_vfs.h"
#include "esp_spiffs.h"
#include "esp_vfs_fat.h"

#define AP_SSID CONFIG_AP_SSID
#define AP_PSSWD CONFIG_AP_PSSWD

static QueueHandle_t client_queue;
const static int client_queue_size = 10;

static const char tag[] = "[SPIFFS example]";
int BLINK_GPIO_1;
int BLINK_GPIO_2;
int BLINK_GPIO_3;
//static const char TAG[] = "Spiffs";

// handles WiFi events
static esp_err_t event_handler(void* ctx, system_event_t* event) {
  const char* TAG = "event_handler";
  switch(event->event_id) {
    case SYSTEM_EVENT_AP_START:
      //ESP_ERROR_CHECK(tcpip_adapter_set_hostname(TCPIP_ADAPTER_IF_STA, "esp32"));
      ESP_LOGI(TAG,"Access Point Started");
      break;
    case SYSTEM_EVENT_AP_STOP:
      ESP_LOGI(TAG,"Access Point Stopped");
      break;
    case SYSTEM_EVENT_AP_STACONNECTED:
      ESP_LOGI(TAG,"STA Connected, MAC=%02x:%02x:%02x:%02x:%02x:%02x AID=%i",
        event->event_info.sta_connected.mac[0],event->event_info.sta_connected.mac[1],
        event->event_info.sta_connected.mac[2],event->event_info.sta_connected.mac[3],
        event->event_info.sta_connected.mac[4],event->event_info.sta_connected.mac[5],
        event->event_info.sta_connected.aid);
      break;
    case SYSTEM_EVENT_AP_STADISCONNECTED:
      ESP_LOGI(TAG,"STA Disconnected, MAC=%02x:%02x:%02x:%02x:%02x:%02x AID=%i",
        event->event_info.sta_disconnected.mac[0],event->event_info.sta_disconnected.mac[1],
        event->event_info.sta_disconnected.mac[2],event->event_info.sta_disconnected.mac[3],
        event->event_info.sta_disconnected.mac[4],event->event_info.sta_disconnected.mac[5],
        event->event_info.sta_disconnected.aid);
      break;
    case SYSTEM_EVENT_AP_PROBEREQRECVED:
      ESP_LOGI(TAG,"AP Probe Received");
      break;
    case SYSTEM_EVENT_AP_STA_GOT_IP6:
      ESP_LOGI(TAG,"Got IP6=%01x:%01x:%01x:%01x",
        event->event_info.got_ip6.ip6_info.ip.addr[0],event->event_info.got_ip6.ip6_info.ip.addr[1],
        event->event_info.got_ip6.ip6_info.ip.addr[2],event->event_info.got_ip6.ip6_info.ip.addr[3]);
      break;
    default:
      ESP_LOGI(TAG,"Unregistered event=%i",event->event_id);
      break;
  }
  return ESP_OK;
}

// sets up WiFi
static void wifi_setup() {
  const char* TAG = "wifi_setup";

  ESP_LOGI(TAG,"starting tcpip adapter");
  tcpip_adapter_init();
  nvs_flash_init();
  ESP_ERROR_CHECK(tcpip_adapter_dhcps_stop(TCPIP_ADAPTER_IF_AP));
  //tcpip_adapter_set_hostname(TCPIP_ADAPTER_IF_AP,"esp32");
  tcpip_adapter_ip_info_t info;
  memset(&info, 0, sizeof(info));
  IP4_ADDR(&info.ip, 192, 168, 4, 1);
  IP4_ADDR(&info.gw, 192, 168, 4, 1);
  IP4_ADDR(&info.netmask, 255, 255, 255, 0);
  ESP_LOGI(TAG,"setting gateway IP");
  ESP_ERROR_CHECK(tcpip_adapter_set_ip_info(TCPIP_ADAPTER_IF_AP, &info));
  //ESP_ERROR_CHECK(tcpip_adapter_set_hostname(TCPIP_ADAPTER_IF_AP,"esp32"));
  //ESP_LOGI(TAG,"set hostname to \"%s\"",hostname);
  ESP_LOGI(TAG,"starting DHCPS adapter");
  ESP_ERROR_CHECK(tcpip_adapter_dhcps_start(TCPIP_ADAPTER_IF_AP));
  //ESP_ERROR_CHECK(tcpip_adapter_set_hostname(TCPIP_ADAPTER_IF_AP,hostname));
  ESP_LOGI(TAG,"starting event loop");
  ESP_ERROR_CHECK(esp_event_loop_init(event_handler, NULL));

  ESP_LOGI(TAG,"initializing WiFi");
  wifi_init_config_t wifi_init_config = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&wifi_init_config));
  ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));

  wifi_config_t wifi_config = {
    .ap = {
      .ssid = AP_SSID,
      .password= AP_PSSWD,
      .channel = 0,
      .authmode = WIFI_AUTH_WPA2_PSK,
      .ssid_hidden = 0,
      .max_connection = 4,
      .beacon_interval = 100
    }
  };

  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
  ESP_ERROR_CHECK(esp_wifi_start());
  ESP_LOGI(TAG,"WiFi set up");
}

// sets up the led for pwm

static void read_json(char *fname)
{
    printf("  file: \"%s\"\n", fname);

    int res;
    char *buf;
    buf = calloc(1024, 1);
    if (buf == NULL) {
        printf("  Error allocating read buffer\n");
        printf("\n");
        return;
    }

    FILE *fd = fopen(fname, "rb");
    if (fd == NULL)
    {
        printf("  Error opening file (%d) %s\n", errno, strerror(errno));
        free(buf);
        printf("\n");
        return;
    }
    res = fread(buf, 1, 200, fd);
    if (res <= 0)
    {
        printf("  Error reading from file\n");
    }
    else
    {
       // printf("  %d bytes read [\n", res);
        buf[res] = '\0';
        printf("%s\n]\n", buf);
    }
   // printf("%s \n", buf);

	//ESP_LOGI(TAG, "Deserialize.....");
	cJSON *root2 = cJSON_Parse(buf);
	ESP_LOGI(tag, "values from JSON after deserialization");
	if (cJSON_GetObjectItem(root2, "pin1")) {
		char *GPIO_1 = cJSON_GetObjectItem(root2,"pin1")->valuestring;
		ESP_LOGI(tag, "pin1=%s",GPIO_1);
		sscanf(GPIO_1, "%d", &BLINK_GPIO_1);
		//BLINK_GPIO_2 = GPIO_2 ;
	}
	if (cJSON_GetObjectItem(root2, "pin2")) {
		char *GPIO_2 = cJSON_GetObjectItem(root2,"pin2")->valuestring;
		ESP_LOGI(tag, "pin2=%s",GPIO_2);
		sscanf(GPIO_2, "%d", &BLINK_GPIO_2);
		//BLINK_GPIO_2 = GPIO_2 ;
	}
	if (cJSON_GetObjectItem(root2, "pin3")) {
		char *GPIO_3 = cJSON_GetObjectItem(root2,"pin3")->valuestring;
		ESP_LOGI(tag, "pin3=%s",GPIO_3);
		sscanf(GPIO_3, "%d", &BLINK_GPIO_3);
		//BLINK_GPIO_3 = GPIO_3 ;
	}

	cJSON_Delete(root2);
	cJSON_free(buf);
    res = fclose(fd);

    gpio_pad_select_gpio(BLINK_GPIO_1);
    gpio_set_direction(BLINK_GPIO_1, GPIO_MODE_OUTPUT);
    gpio_pad_select_gpio(BLINK_GPIO_2);
    gpio_set_direction(BLINK_GPIO_2, GPIO_MODE_OUTPUT);
    gpio_pad_select_gpio(BLINK_GPIO_3);
    gpio_set_direction(BLINK_GPIO_3, GPIO_MODE_OUTPUT);


    while(1) {

	printf("Turning off the LED2\n");
    gpio_set_level(BLINK_GPIO_1, 0);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
	printf("Turning on the LED2\n");
	gpio_set_level(BLINK_GPIO_1, 1);
    vTaskDelay(1000 / portTICK_PERIOD_MS);

    printf("Turning off the LED3\n");
    gpio_set_level(BLINK_GPIO_2, 0);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    printf("Turning on the LED3\n");
    gpio_set_level(BLINK_GPIO_2, 1);
    vTaskDelay(1000 / portTICK_PERIOD_MS);

    printf("Turning off the LED4\n");
    gpio_set_level(BLINK_GPIO_3, 0);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    printf("Turning on the LED4\n");
    gpio_set_level(BLINK_GPIO_3, 1);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    }

}


const char *jsondata ;
//size_t i = message
// handles websocket events
void websocket_callback(uint8_t num,WEBSOCKET_TYPE_t type,char* msg,uint64_t len) {
  const static char* TAG = "websocket_callback";
  int value;

  switch(type) {
    case WEBSOCKET_CONNECT:
      ESP_LOGI(TAG,"client %i connected!",num);
      //ws_server_send_text_client(num,msg,len)
      break;
    case WEBSOCKET_DISCONNECT_EXTERNAL:
      ESP_LOGI(TAG,"client %i sent a disconnect message",num);

      break;
    case WEBSOCKET_DISCONNECT_INTERNAL:
      ESP_LOGI(TAG,"client %i was disconnected",num);
      break;
    case WEBSOCKET_DISCONNECT_ERROR:
      ESP_LOGI(TAG,"client %i was disconnected due to an error",num);

      break;
    case WEBSOCKET_TEXT:
     // ws_server_send_text_all_from_callback(message,108); // broadcast it!
    	printf("value from client  %s \n ", msg);
    	jsondata = msg;
    	ESP_LOGI(TAG, "JSON variables are \n%s",msg);
    		FILE* f = fopen("/spiffs/variable.json", "w");
    		if (f == NULL) {
    			ESP_LOGE(TAG, "Failed to open file for writing");
    		    return;
    		}
    		fprintf(f, msg);
    		fclose(f);
    		read_json("/spiffs/variable.json");
     // ws_server_send_text_all(message,108);

      break;
    case WEBSOCKET_BIN:
      ESP_LOGI(TAG,"client %i sent binary message of size %i:\n%s",num,(uint32_t)len,msg);
      break;
    case WEBSOCKET_PING:
      ESP_LOGI(TAG,"client %i pinged us with message of size %i:\n%s",num,(uint32_t)len,msg);
      break;
    case WEBSOCKET_PONG:
      ESP_LOGI(TAG,"client %i responded to the ping",num);
      break;
  }
}

// serves any clients
static void http_serve(struct netconn *conn) {
  const static char* TAG = "http_server";
  const static char HTML_HEADER[] = "HTTP/1.1 200 OK\nContent-type: text/html\n\n";
  const static char ERROR_HEADER[] = "HTTP/1.1 404 Not Found\nContent-type: text/html\n\n";
  const static char JS_HEADER[] = "HTTP/1.1 200 OK\nContent-type: text/javascript\n\n";
  const static char CSS_HEADER[] = "HTTP/1.1 200 OK\nContent-type: text/css\n\n";
  //const static char PNG_HEADER[] = "HTTP/1.1 200 OK\nContent-type: image/png\n\n";
  const static char ICO_HEADER[] = "HTTP/1.1 200 OK\nContent-type: image/x-icon\n\n";
  //const static char PDF_HEADER[] = "HTTP/1.1 200 OK\nContent-type: application/pdf\n\n";
  //const static char EVENT_HEADER[] = "HTTP/1.1 200 OK\nContent-Type: text/event-stream\nCache-Control: no-cache\nretry: 3000\n\n";
  struct netbuf* inbuf;
  static char* buf;
  static uint16_t buflen;
  static err_t err;

  // default page
  extern const uint8_t root_html_start[] asm("_binary_root_html_start");
  extern const uint8_t root_html_end[] asm("_binary_root_html_end");
  const uint32_t root_html_len = root_html_end - root_html_start;

  // test.js
  extern const uint8_t test_js_start[] asm("_binary_test_js_start");
  extern const uint8_t test_js_end[] asm("_binary_test_js_end");
  const uint32_t test_js_len = test_js_end - test_js_start;

  // test.css
  extern const uint8_t test_css_start[] asm("_binary_test_css_start");
  extern const uint8_t test_css_end[] asm("_binary_test_css_end");
  const uint32_t test_css_len = test_css_end - test_css_start;

  // favicon.ico
  extern const uint8_t favicon_ico_start[] asm("_binary_favicon_ico_start");
  extern const uint8_t favicon_ico_end[] asm("_binary_favicon_ico_end");
  const uint32_t favicon_ico_len = favicon_ico_end - favicon_ico_start;

  // error page
  extern const uint8_t error_html_start[] asm("_binary_error_html_start");
  extern const uint8_t error_html_end[] asm("_binary_error_html_end");
  const uint32_t error_html_len = error_html_end - error_html_start;

  netconn_set_recvtimeout(conn,1000); // allow a connection timeout of 1 second
  ESP_LOGI(TAG,"reading from client...");
  err = netconn_recv(conn, &inbuf);
  ESP_LOGI(TAG,"read from client");
  if(err==ERR_OK) {
    netbuf_data(inbuf, (void**)&buf, &buflen);
    if(buf) {
      // default page
      if     (strstr(buf,"GET / ")
          && !strstr(buf,"Upgrade: websocket")) {
        ESP_LOGI(TAG,"Sending /");
        netconn_write(conn, HTML_HEADER, sizeof(HTML_HEADER)-1,NETCONN_NOCOPY);
        netconn_write(conn, root_html_start,root_html_len,NETCONN_NOCOPY);
        netconn_close(conn);
        netconn_delete(conn);
        netbuf_delete(inbuf);
      }

      // default page websocket
      else if(strstr(buf,"GET / ")
           && strstr(buf,"Upgrade: websocket")) {
        ESP_LOGI(TAG,"Requesting websocket on /");
        ws_server_add_client(conn,buf,buflen,"/",websocket_callback);
        netbuf_delete(inbuf);
      }

      else if(strstr(buf,"GET /test.js ")) {
        ESP_LOGI(TAG,"Sending /test.js");
        netconn_write(conn, JS_HEADER, sizeof(JS_HEADER)-1,NETCONN_NOCOPY);
        netconn_write(conn, test_js_start, test_js_len,NETCONN_NOCOPY);
      //  netconn_write(conn, message, sizeof(message)-1,NETCONN_NOCOPY);
        netconn_close(conn);
        netconn_delete(conn);
        netbuf_delete(inbuf);
      }

      else if(strstr(buf,"GET /test.css ")) {
        ESP_LOGI(TAG,"Sending /test.css");
        netconn_write(conn, CSS_HEADER, sizeof(CSS_HEADER)-1,NETCONN_NOCOPY);
        netconn_write(conn, test_css_start, test_css_len,NETCONN_NOCOPY);
        netconn_close(conn);
        netconn_delete(conn);
        netbuf_delete(inbuf);
      }

      else if(strstr(buf,"GET /favicon.ico ")) {
        ESP_LOGI(TAG,"Sending favicon.ico");
        netconn_write(conn,ICO_HEADER,sizeof(ICO_HEADER)-1,NETCONN_NOCOPY);
        netconn_write(conn,favicon_ico_start,favicon_ico_len,NETCONN_NOCOPY);
        netconn_close(conn);
        netconn_delete(conn);
        netbuf_delete(inbuf);
      }

      else if(strstr(buf,"GET /")) {
        ESP_LOGI(TAG,"Unknown request, sending error page: %s",buf);
        netconn_write(conn, ERROR_HEADER, sizeof(ERROR_HEADER)-1,NETCONN_NOCOPY);
        netconn_write(conn, error_html_start, error_html_len,NETCONN_NOCOPY);
        netconn_close(conn);
        netconn_delete(conn);
        netbuf_delete(inbuf);
      }

      else {
        ESP_LOGI(TAG,"Unknown request");
        netconn_close(conn);
        netconn_delete(conn);
        netbuf_delete(inbuf);
      }
    }
    else {
      ESP_LOGI(TAG,"Unknown request (empty?...)");
      netconn_close(conn);
      netconn_delete(conn);
      netbuf_delete(inbuf);
    }
  }
  else { // if err==ERR_OK
    ESP_LOGI(TAG,"error on read, closing connection");
    netconn_close(conn);
    netconn_delete(conn);
    netbuf_delete(inbuf);
  }
}

// handles clients when they first connect. passes to a queue
static void server_task(void* pvParameters) {
  const static char* TAG = "server_task";
  struct netconn *conn, *newconn;
  static err_t err;
  client_queue = xQueueCreate(client_queue_size,sizeof(struct netconn*));

  conn = netconn_new(NETCONN_TCP);
  netconn_bind(conn,NULL,80);
  netconn_listen(conn);
  ESP_LOGI(TAG,"server listening");
  do {
    err = netconn_accept(conn, &newconn);
    ESP_LOGI(TAG,"new client");
    if(err == ERR_OK) {
      xQueueSendToBack(client_queue,&newconn,portMAX_DELAY);
      //http_serve(newconn);
    }
  } while(err == ERR_OK);
  netconn_close(conn);
  netconn_delete(conn);
  ESP_LOGE(TAG,"task ending, rebooting board");
  esp_restart();
}

// receives clients from queue, handles them
static void server_handle_task(void* pvParameters) {
  const static char* TAG = "server_handle_task";
  struct netconn* conn;
  ESP_LOGI(TAG,"task starting");
  for(;;) {
    xQueueReceive(client_queue,&conn,portMAX_DELAY);
    if(!conn) continue;
    http_serve(conn);
  }
  vTaskDelete(NULL);
}

void app_main() {
	esp_vfs_spiffs_conf_t conf = {
	      .base_path = "/spiffs",
	      .partition_label = NULL,
	      .max_files = 5,
	      .format_if_mount_failed = true
	    };

	    esp_err_t ret = esp_vfs_spiffs_register(&conf);

	    if (ret != ESP_OK) {
	        if (ret == ESP_FAIL) {
	            ESP_LOGE(tag, "Failed to mount or format filesystem");
	        } else if (ret == ESP_ERR_NOT_FOUND) {
	            ESP_LOGE(tag, "Failed to find SPIFFS partition");
	        } else {
	            ESP_LOGE(tag, "Failed to initialize SPIFFS (%d)", ret);
	        }

	    }

  wifi_setup();
  ws_server_start();
  xTaskCreate(&server_task,"server_task",3000,NULL,9,NULL);
  xTaskCreate(&server_handle_task,"server_handle_task",4000,NULL,6,NULL);

  //xTaskCreate(&count_task,"count_task",6000,NULL,2,NULL);
}
