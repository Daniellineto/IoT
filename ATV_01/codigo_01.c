// Importando as bibliotecas
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

// Definindo componentes
#define LED1 GPIO_NUM_15
#define LED2 GPIO_NUM_16
#define LED3 GPIO_NUM_17
#define LED4 GPIO_NUM_18

static uint16_t v = 0;
const gpio_num_t leds[] = {LED1, LED2, LED3, LED4};

// Configuração GPIO
void setup(void)
{
    // LEDs
    gpio_config_t io_leds = {
        .pin_bit_mask = (1ULL << LED1) | (1ULL << LED2) | (1ULL << LED3) | (1ULL << LED4),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = 0,
        .pull_down_en = 0,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_leds);
}

// LOOP
void app_main(void)
{
  setup();
  //usado na varredura
  int indiceAtual = 0;
  int direcao = 1;

  while(true)
  {

    // ==========================================
    // FASE 1 - Contador Binario (0 a 15)
    // ==========================================
    printf("Iniciando Fase 1: Contador Binario\n");
    for (v = 0; v<=15; v++)
    {
      gpio_set_level(LED1, v & 0x1);
      gpio_set_level(LED2, (v >> 1) & 0x1);
      gpio_set_level(LED3, (v >> 2) & 0x1);
      gpio_set_level(LED4, (v >> 3) & 0x1);
      vTaskDelay(pdMS_TO_TICKS(500));
    }
    printf("Finalizando Fase 1: Contador Binario\n");
    vTaskDelay(pdMS_TO_TICKS(500));
    
    // ==========================================
    // FASE 2 - Varredura
    // ==========================================
    printf("Iniciando Fase 2: Varredura\n");
    // quantas vezes ele vai passar de um led para o outro
    for (int passos = 0; passos <=6; passos++)
    {
      
      for(int i = 0; i <4; i++)
      {
        gpio_set_level(leds[i], 0);
      }
      
      gpio_set_level(leds[indiceAtual], 1);
      indiceAtual += direcao; 

      // faz a ida da varredura
      if (indiceAtual >= 3)
      {
        indiceAtual = 3;
        direcao = -1;
      }
      // faz a volta da varredura
      else if (indiceAtual <= 0)
      {
        indiceAtual = 0;
        direcao = 1;
      }

      vTaskDelay(pdMS_TO_TICKS(500));
    }
    printf("Finalizado Fase 2: Varredura\n");
    vTaskDelay(pdMS_TO_TICKS(500));
  }
}

