/**
 * ==============================================================================
 * PROJETO CARDIOIA - A NOVA ERA DA CARDIOLOGIA INTELIGENTE (FIAP)
 * Arquivo: config.h
 * Descricao: Configuracao de conexao de rede, MQTT (HiveMQ Cloud) e limiares.
 * ==============================================================================
 */

#ifndef CARDIOIA_CONFIG_H
#define CARDIOIA_CONFIG_H

// =============================================================================
// 1. CONFIGURACOES DE CONEXAO WI-FI
// =============================================================================
// "Wokwi-GUEST" e a rede padrao do simulador Wokwi.
// Para hardware fisico ESP32, altere para seu SSID e Senha locais.
#define WIFI_SSID           "Wokwi-GUEST"
#define WIFI_PASSWORD       ""

// =============================================================================
// 2. CONFIGURACOES DO BROKER MQTT (HIVEMQ CLOUD / BROKER HIVEMQ)
// =============================================================================
// Mantenha true para HiveMQ Cloud com criptografia TLS (Porta 8883).
// Altere para false se desejar utilizar o broker publico aberto broker.hivemq.com (Porta 1883).
#define MQTT_USE_TLS        true

#if MQTT_USE_TLS
  // Substitua pela URL do seu cluster criado no HiveMQ Cloud
  // Exemplo: "b49a1d5c7f8e4a908d123456789abcde.s1.eu.hivemq.cloud"
  #define MQTT_BROKER       "SEU_CLUSTER_HIVEMQ.s1.eu.hivemq.cloud"
  #define MQTT_PORT         8883
#else
  // Broker publico para testes imediatos sem autenticacao
  #define MQTT_BROKER       "broker.hivemq.com"
  #define MQTT_PORT         1883
#endif

// Credenciais configuradas no HiveMQ Cloud Console (Access Management)
#define MQTT_USERNAME       "cardioia_user"
#define MQTT_PASSWORD       "CardioIA@2026"

// Identificador unico do dispositivo
#define MQTT_CLIENT_ID      "CardioIA_ESP32_Patient01"

// =============================================================================
// 3. IDENTIFICACAO DO PACIENTE E TOPICOS MQTT
// =============================================================================
#define PATIENT_ID          "paciente01"
#define DEVICE_ID           "ESP32-CARDIOIA-001"

// Estrutura de topicos padronizada
#define TOPIC_BASE          "cardioia/" PATIENT_ID
#define TOPIC_TELEMETRY     TOPIC_BASE "/telemetria"
#define TOPIC_BPM           TOPIC_BASE "/bpm"
#define TOPIC_TEMPERATURE   TOPIC_BASE "/temperatura"
#define TOPIC_HUMIDITY      TOPIC_BASE "/umidade"
#define TOPIC_STATUS        TOPIC_BASE "/status"
#define TOPIC_ALERTS        TOPIC_BASE "/alertas"
#define TOPIC_COMMANDS      TOPIC_BASE "/comandos"

// Mensagens Last Will and Testament (LWT)
#define LWT_PAYLOAD_OFFLINE "{\"status\":\"OFFLINE\",\"reason\":\"Perda de conexao do dispositivo\"}"
#define LWT_PAYLOAD_ONLINE  "{\"status\":\"ONLINE\",\"device\":\"" DEVICE_ID "\"}"

// =============================================================================
// 4. MAPEAMENTO DE PINOS DO HARDWARE (ESP32)
// =============================================================================
#define PIN_DHT             15    // Pino digital do sensor DHT22
#define PIN_POT_BPM         34    // Entrada analogica do Potenciometro (ADC1 - seguro com Wi-Fi)
#define PIN_LED_STATUS      2     // LED Verde (Status de operacao / Conectado)
#define PIN_LED_ALERT       4     // LED Vermelho (Alerta critico)
#define PIN_BUZZER          5     // Buzzer Piezo (Alarme sonoro em arritmia/febre)

// =============================================================================
// 5. INTERVALOS E TEMPOS DE AMOSTRAGEM
// =============================================================================
#define SENSOR_READ_INTERVAL_MS     3000   // Leitura dos sensores a cada 3s
#define MQTT_PUBLISH_INTERVAL_MS    3000   // Envio de telemetria a cada 3s
#define SYNC_INTERVAL_MS            150    // Cadencia de envio dos dados offline acumulados
#define WIFI_RETRY_INTERVAL_MS      5000   // Intervalo de tentativa de conexao Wi-Fi
#define MQTT_RETRY_INTERVAL_MS      5000   // Intervalo de reconexao MQTT

// =============================================================================
// 6. LIMITES CLINICOS PARA DISPARO DE ALERTAS (EDGE COMPUTING)
// =============================================================================
#define BPM_MIN_CRITICAL            40     // Bradicardia severa
#define BPM_MIN_NORMAL              60     // Limite inferior normal
#define BPM_MAX_NORMAL              100    // Limite superior normal
#define BPM_ALERT_THRESHOLD         120    // Taquicardia severa (Alarme disparado)
#define BPM_MAX_CRITICAL            180    // Limite maximo simulado

#define TEMP_MIN_CRITICAL           35.0f  // Hipotermia
#define TEMP_NORMAL_MIN             36.0f  // Normal inferior
#define TEMP_NORMAL_MAX             37.5f  // Normal superior
#define TEMP_ALERT_THRESHOLD        38.0f  // Febre alta (Alarme disparado)

// =============================================================================
// 7. CONFIGURACOES DE ARMAZENAMENTO RESILIENTE (EDGE & SPIFFS)
// =============================================================================
#define MAX_OFFLINE_RECORDS         60     // Capacidade da fila circular em RAM (3 min @ 3s)
#define SPIFFS_FILE_PATH            "/cardioia_offline.dat"

#endif // CARDIOIA_CONFIG_H
