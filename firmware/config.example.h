/**
 * ==============================================================================
 * PROJETO CARDIOIA - A NOVA ERA DA CARDIOLOGIA INTELIGENTE (FIAP)
 * Arquivo: config.example.h
 * Descricao: Template de configuracao de credenciais de rede, MQTT e limiares.
 * Instrucoes: Renomeie ou copie este arquivo como "config.h" e insira suas
 *             credenciais do HiveMQ Cloud e Wi-Fi.
 * ==============================================================================
 */

#ifndef CARDIOIA_CONFIG_H
#define CARDIOIA_CONFIG_H

// =============================================================================
// 1. CONFIGURACOES DE CONEXAO WI-FI
// =============================================================================
// No simulador Wokwi, utilize "Wokwi-GUEST" sem senha ("").
// Em hardware fisico real, substitua pelo SSID e senha da sua rede local (2.4GHz).
#define WIFI_SSID           "Wokwi-GUEST"
#define WIFI_PASSWORD       ""

// =============================================================================
// 2. CONFIGURACOES DO BROKER MQTT (HIVEMQ CLOUD)
// =============================================================================
// Altere para true se for conectar ao HiveMQ Cloud com criptografia TLS (Porta 8883).
// Altere para false para brokers publicos sem TLS (ex: broker.hivemq.com porta 1883).
#define MQTT_USE_TLS        true

#if MQTT_USE_TLS
  // URL do seu cluster no HiveMQ Cloud (ex: "xxxxxxxxxxxxxx.s1.eu.hivemq.cloud")
  #define MQTT_BROKER       "SEU_CLUSTER_HIVEMQ.s1.eu.hivemq.cloud"
  #define MQTT_PORT         8883
#else
  // Broker publico para testes rapidos sem certificado/credenciais
  #define MQTT_BROKER       "broker.hivemq.com"
  #define MQTT_PORT         1883
#endif

// Credenciais de acesso configuradas no painel do HiveMQ Cloud (Access Management)
#define MQTT_USERNAME       "cardioia_user"
#define MQTT_PASSWORD       "SuaSenhaForte123"

// Identificador unico do cliente MQTT
#define MQTT_CLIENT_ID      "CardioIA_ESP32_Patient01"

// =============================================================================
// 3. IDENTIFICACAO DO PACIENTE E TOPICOS MQTT
// =============================================================================
#define PATIENT_ID          "paciente01"
#define DEVICE_ID           "ESP32-CARDIOIA-001"

// Topicos de publicacao estruturados
#define TOPIC_BASE          "cardioia/" PATIENT_ID
#define TOPIC_TELEMETRY     TOPIC_BASE "/telemetria"
#define TOPIC_BPM           TOPIC_BASE "/bpm"
#define TOPIC_TEMPERATURE   TOPIC_BASE "/temperatura"
#define TOPIC_HUMIDITY      TOPIC_BASE "/umidade"
#define TOPIC_STATUS        TOPIC_BASE "/status"
#define TOPIC_ALERTS        TOPIC_BASE "/alertas"
#define TOPIC_COMMANDS      TOPIC_BASE "/comandos"

// Mensagens de Last Will and Testament (LWT)
#define LWT_PAYLOAD_OFFLINE "{\"status\":\"OFFLINE\",\"reason\":\"Desconexao inesperada do vestivel\"}"
#define LWT_PAYLOAD_ONLINE  "{\"status\":\"ONLINE\",\"device\":\"" DEVICE_ID "\"}"

// =============================================================================
// 4. MAPEAMENTO DE PINOS DO HARDWARE (ESP32)
// =============================================================================
// Sensor de Temperatura e Umidade DHT22
#define PIN_DHT             15

// Potenciometro para simulacao de Batimentos Cardiacos (BPM)
// IMPORTANTE: Utilizar canal ADC1 (GPIO 32 a 39). ADC2 e bloqueado pelo Wi-Fi do ESP32!
#define PIN_POT_BPM         34

// LEDs de Sinalizacao Visual
#define PIN_LED_STATUS      2     // LED Verde: Operacao Normal / Conectado
#define PIN_LED_ALERT       4     // LED Vermelho: Alerta Critico Ativado

// Buzzer Piezoeletrico de Alarme Sonoro
#define PIN_BUZZER          5     // Alarme sonoro em arritmias ou febre grave

// =============================================================================
// 5. INTERVALOS E TEMPOS DE AMOSTRAGEM
// =============================================================================
#define SENSOR_READ_INTERVAL_MS     3000   // Leitura dos sensores a cada 3 segundos
#define MQTT_PUBLISH_INTERVAL_MS    3000   // Envio de telemetria a cada 3 segundos
#define SYNC_INTERVAL_MS            100    // Delay entre envios da fila offline (evita flood)
#define WIFI_RETRY_INTERVAL_MS      5000   // Tentativa de reconexao Wi-Fi
#define MQTT_RETRY_INTERVAL_MS      5000   // Tentativa de reconexao MQTT

// =============================================================================
// 6. LIMITES CLINICOS PARA DISPARO DE ALERTAS (EDGE COMPUTING)
// =============================================================================
#define BPM_MIN_CRITICAL            40     // Bradicardia severa
#define BPM_MIN_NORMAL              60     // Limite inferior repouso
#define BPM_MAX_NORMAL              100    // Limite superior repouso
#define BPM_ALERT_THRESHOLD         120    // Taquicardia importante / Alarme
#define BPM_MAX_CRITICAL            180    // Taquicardia extrema

#define TEMP_MIN_CRITICAL           35.0f  // Hipotermia
#define TEMP_NORMAL_MIN             36.0f  // Temperatura basal minima
#define TEMP_NORMAL_MAX             37.5f  // Temperatura basal maxima
#define TEMP_ALERT_THRESHOLD        38.0f  // Febre / Alarme clinico

// =============================================================================
// 7. CONFIGURACOES DE ARMAZENAMENTO RESILIENTE (EDGE & SPIFFS)
// =============================================================================
#define MAX_OFFLINE_RECORDS         50     // Capacidade da fila circular em memoria (RAM)
#define SPIFFS_FILE_PATH            "/cardioia_offline.dat"

#endif // CARDIOIA_CONFIG_H
