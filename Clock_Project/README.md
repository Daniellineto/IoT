# 🕒 Clock ESP32-C6 (NTP.br)

Este projeto implementa um relógio sincronizado via Wi-Fi no microcontrolador **ESP32-C6**, utilizando o framework **ESP-IDF** e os servidores Stratum 1 do **ntp.br**.

## Funcionalidades
* Conexão Wi-Fi em modo Station.
* Sincronização automática de data/hora via protocolo SNTP.
* Configuração de fuso horário para **Horário de Brasília (UTC-3)**.
* Saída formatada no monitor serial.

## Como usar

1. **Configuração:** No arquivo `main.c`, insira suas credenciais:
```c
#define WIFI_SSID "wifi-iot-2.4"
#define WIFI_PASS "iot-2026.1"
```

2. **Build & Flash:**
```bash
idf.py set-target esp32c6
idf.py build
idf.py flash
idf.py monitor
```

**Estrutura de Arquivos**
* main/: Código-fonte (main.c) e lógica do SNTP.
* sdkconfig: Configurações do framework.
* .gitignore: Ignora a pasta build/ e arquivos temporários.

---
Desenvolvido por *Daniel Lima Neto*
