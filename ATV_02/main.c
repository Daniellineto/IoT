#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_sntp.h"

// ============================================================================
// Configurações da Rede Wi-Fi
// ============================================================================
#define WIFI_SSID      "ESP_TEST"
#define WIFI_PASS      "dkut2147"
#define MAXIMUM_RETRY  5

// ============================================================================
// Variáveis Globais e Definições
// ============================================================================
static const char *TAG = "WIFI_SNTP";
static EventGroupHandle_t s_wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static int s_retry_num = 0;

// ============================================================================
// Handler de Eventos Wi-Fi e IP
// ============================================================================
static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "Tentando reconectar ao Wi-Fi...");
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG, "Falha ao conectar ao Wi-Fi.");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Endereço IP recebido: " IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

// ============================================================================
// Inicialização do Wi-Fi em Modo Station
// ============================================================================
void wifi_init_sta(void) {
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL, &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL, &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_init_sta finalizado. Aguardando conexão...");

    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT, pdFALSE, pdFALSE, portMAX_DELAY);

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Conectado à rede SSID:%s", WIFI_SSID);
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG, "Falha ao conectar à rede SSID:%s", WIFI_SSID);
    } else {
        ESP_LOGE(TAG, "Evento inesperado no Wi-Fi");
    }
}

// ============================================================================
// Callback de Sincronização SNTP
// ============================================================================
void time_sync_notification_cb(struct timeval *tv) {
    ESP_LOGI(TAG, "Sincronização de tempo concluída com sucesso!");
}

// ============================================================================
// Inicialização e Obtenção do Tempo via SNTP (ntp.br)
// ============================================================================
static void initialize_sntp(void) {
    ESP_LOGI(TAG, "Inicializando SNTP com servidores ntp.br...");
    
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    
    // Configurando os servidores do ntp.br
    esp_sntp_setservername(0, "a.st1.ntp.br");
    esp_sntp_setservername(1, "b.st1.ntp.br");
    esp_sntp_setservername(2, "c.st1.ntp.br");
    
    sntp_set_time_sync_notification_cb(time_sync_notification_cb);
    esp_sntp_init();
}

static void obtain_time(void) {
    initialize_sntp();

    // Aguarda a sincronização ser concluída
    time_t now = 0;
    struct tm timeinfo = { 0 };
    int retry = 0;
    const int retry_count = 15;
    
    while (sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && ++retry < retry_count) {
        ESP_LOGI(TAG, "Aguardando sincronização do sistema de tempo... (%d/%d)", retry, retry_count);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }
    time(&now);
    localtime_r(&now, &timeinfo);
}

// ============================================================================
// Aplicação Principal
// ============================================================================
void app_main(void) {
    // Inicializa o NVS (necessário para o Wi-Fi)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Conecta ao Wi-Fi
    wifi_init_sta();

    // Obtém a hora via SNTP
    obtain_time();

    // Configura o fuso horário para Brasília (UTC-3)
    // "<-03>3" é o formato POSIX padrão para fuso fixo -3h sem horário de verão.
    setenv("TZ", "<-03>3", 1);
    tzset();

    // Loop principal imprimindo a data e hora atual
    time_t now;
    struct tm timeinfo;
    char strftime_buf[64];

    while (1) {
        time(&now);
        localtime_r(&now, &timeinfo);
        
        // Formata a string de tempo: "DD-MM-YYYY HH:MM:SS"
        strftime(strftime_buf, sizeof(strftime_buf), "%d-%m-%Y %H:%M:%S", &timeinfo);
        ESP_LOGI(TAG, "Data/Hora Atual em Brasília: %s", strftime_buf);
        
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}