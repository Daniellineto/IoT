#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "driver/ledc.h"
#include "esp_timer.h"

// Definição de pinos
#define BUTTON_A_GPIO     11
#define BUTTON_B_GPIO     14
#define LED1_GPIO         15
#define LED2_GPIO         16
#define BUZZER_GPIO       42

// UART
#define UART_NUM          UART_NUM_0
#define BUF_SIZE          1024

// PWM
#define LEDC_TIMER        LEDC_TIMER_0
#define LEDC_MODE         LEDC_LOW_SPEED_MODE
#define LEDC_LED_CHANNEL  LEDC_CHANNEL_1
#define LEDC_BUZZ_CHANNEL LEDC_CHANNEL_0
#define LEDC_DUTY_RES     LEDC_TIMER_10_BIT
#define LEDC_FREQUENCY    2000

// Variáveis
static volatile bool button_b_enabled = true;
static volatile bool buzzer_active = false;
static uint64_t buzzer_start_time = 0;
static volatile uint64_t last_button_a_time = 0;

static QueueHandle_t gpio_evt_queue = NULL;

// ================= ISR =================

static void IRAM_ATTR button_a_isr_handler(void* arg) {
    uint64_t now = esp_timer_get_time();
    if (now - last_button_a_time > 200000) {
        last_button_a_time = now;
        uint32_t gpio_num = (uint32_t) arg;
        xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
    }
}

static void IRAM_ATTR button_b_isr_handler(void* arg) {
    uint32_t gpio_num = (uint32_t) arg;
    xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
}

// ================= GPIO =================

static void configure_gpio(void) {

    gpio_config_t io_conf = {};

    // Botões
    io_conf.intr_type = GPIO_INTR_NEGEDGE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << BUTTON_A_GPIO) | (1ULL << BUTTON_B_GPIO);
    io_conf.pull_up_en = 0;
    io_conf.pull_down_en = 0;

    gpio_config(&io_conf);

    // LED1
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL << LED1_GPIO);

    gpio_config(&io_conf);

    gpio_set_level(LED1_GPIO, 0);
}

// ================= PWM =================

static void configure_pwm(void) {

    ledc_timer_config_t ledc_timer = {
        .speed_mode = LEDC_MODE,
        .timer_num = LEDC_TIMER,
        .duty_resolution = LEDC_DUTY_RES,
        .freq_hz = LEDC_FREQUENCY,
        .clk_cfg = LEDC_AUTO_CLK
    };

    ledc_timer_config(&ledc_timer);

    // BUZZER
    ledc_channel_config_t buzzer_channel = {
        .gpio_num = BUZZER_GPIO,
        .speed_mode = LEDC_MODE,
        .channel = LEDC_BUZZ_CHANNEL,
        .timer_sel = LEDC_TIMER,
        .duty = 0,
        .hpoint = 0
    };

    ledc_channel_config(&buzzer_channel);

    // LED2
    ledc_channel_config_t led_channel = {
        .gpio_num = LED2_GPIO,
        .speed_mode = LEDC_MODE,
        .channel = LEDC_LED_CHANNEL,
        .timer_sel = LEDC_TIMER,
        .duty = 0,
        .hpoint = 0
    };

    ledc_channel_config(&led_channel);
}

// ================= UART =================

static void configure_uart(void) {

    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    uart_driver_install(UART_NUM, BUF_SIZE * 2, 0, 0, NULL, 0);
    uart_param_config(UART_NUM, &uart_config);
}

// ================= INTERRUPTS =================

static void configure_interrupts(void) {

    gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));

    gpio_install_isr_service(0);

    gpio_isr_handler_add(BUTTON_A_GPIO, button_a_isr_handler, (void*) BUTTON_A_GPIO);
    gpio_isr_handler_add(BUTTON_B_GPIO, button_b_isr_handler, (void*) BUTTON_B_GPIO);
}

// ================= FADING =================

static void fade_led(void) {

    for (int duty = 0; duty < 1023; duty += 20) {

        ledc_set_duty(LEDC_MODE, LEDC_LED_CHANNEL, duty);
        ledc_update_duty(LEDC_MODE, LEDC_LED_CHANNEL);

        vTaskDelay(10 / portTICK_PERIOD_MS);
    }

    for (int duty = 1023; duty > 0; duty -= 20) {

        ledc_set_duty(LEDC_MODE, LEDC_LED_CHANNEL, duty);
        ledc_update_duty(LEDC_MODE, LEDC_LED_CHANNEL);

        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}

static void fade_buzzer(void)
{
    int duty = 512; // 50% duty

    // subida de frequência
    for (int freq = 500; freq <= 3000; freq += 50)
    {
        ledc_set_freq(LEDC_LOW_SPEED_MODE, LEDC_TIMER_0, freq);

        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);

        vTaskDelay(15 / portTICK_PERIOD_MS);
    }

    // descida de frequência
    for (int freq = 3000; freq >= 500; freq -= 50)
    {
        ledc_set_freq(LEDC_LOW_SPEED_MODE, LEDC_TIMER_0, freq);

        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);

        vTaskDelay(15 / portTICK_PERIOD_MS);
    }
}

// ================= TASKS =================

static void gpio_task(void* arg) {

    uint32_t io_num;

    while (1) {

        if (xQueueReceive(gpio_evt_queue, &io_num, portMAX_DELAY)) {

            if (io_num == BUTTON_A_GPIO) {

                if (gpio_get_level(BUTTON_A_GPIO) == 0) {

                    static bool led1_state = false;
                    led1_state = !led1_state;

                    gpio_set_level(LED1_GPIO, led1_state);

                    printf("Botão A: LED1 %s\n", led1_state ? "LIGADO" : "DESLIGADO");
                }
            }

            else if (io_num == BUTTON_B_GPIO) {

                if (gpio_get_level(BUTTON_B_GPIO) == 0 && button_b_enabled && !buzzer_active) {

                    buzzer_active = true;
                    buzzer_start_time = esp_timer_get_time();

                    printf("Botão B: Buzzer ativado\n");
                }
            }
        }
    }
}

static void buzzer_task(void* arg)
{
    while (1)
    {
        if (buzzer_active)
        {
            fade_buzzer();

            uint64_t now = esp_timer_get_time();

            if (now - buzzer_start_time >= 1500000)
            {
                ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0);
                ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);

                buzzer_active = false;

                printf("Buzzer desativado\n");
            }
        }

        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}

static void led_task(void* arg) {

    while (1) {

        fade_led();

        vTaskDelay(50 / portTICK_PERIOD_MS);
    }
}

static void uart_task(void* arg) {

    uint8_t data[BUF_SIZE];

    printf("Comandos UART:\n");
    printf("'a' = desativa Botão B\n");
    printf("'b' = ativa Botão B\n\n");

    while (1) {

        int len = uart_read_bytes(UART_NUM, data, BUF_SIZE - 1, 20 / portTICK_PERIOD_MS);

        if (len > 0) {

            data[len] = '\0';

            for (int i = 0; i < len; i++) {

                if (data[i] == 'a' || data[i] == 'A') {

                    button_b_enabled = false;
                    printf("Botão B DESATIVADO\n");
                }

                else if (data[i] == 'b' || data[i] == 'B') {

                    button_b_enabled = true;
                    printf("Botão B ATIVADO\n");
                }
            }
        }
    }
}

// ================= MAIN =================

void app_main(void) {

    configure_gpio();
    configure_interrupts();
    configure_uart();
    configure_pwm();

    xTaskCreate(gpio_task, "gpio_task", 2048, NULL, 10, NULL);
    xTaskCreate(buzzer_task, "buzzer_task", 2048, NULL, 9, NULL);
    xTaskCreate(led_task, "led_task", 2048, NULL, 8, NULL);
    xTaskCreate(uart_task, "uart_task", 4096, NULL, 7, NULL);

    printf("Sistema inicializado.\n");
}
