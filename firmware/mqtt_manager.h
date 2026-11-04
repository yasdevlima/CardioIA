/**
 * ==============================================================================
 * PROJETO CARDIOIA - A NOVA ERA DA CARDIOLOGIA INTELIGENTE (FIAP)
 * Arquivo: mqtt_manager.h
 * Descricao: Gerenciador de Comunicacao de Rede Wi-Fi e Protocolo MQTT
 *            (Compativel com HiveMQ Cloud via TLS/SSL e broker aberto).
 * ==============================================================================
 */

#ifndef CARDIOIA_MQTT_MANAGER_H
#define CARDIOIA_MQTT_MANAGER_H

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "config.h"
#include "sensors.h"
#include "offline_manager.h"

class MqttManager {
private:
  WiFiClient standardClient;
  WiFiClientSecure secureClient;
  PubSubClient client;

  unsigned long lastWifiRetryMs;
  unsigned long lastMqttRetryMs;
  bool isConnectedState;

public:
  MqttManager() : lastWifiRetryMs(0), lastMqttRetryMs(0), isConnectedState(false) {}

  void begin() {
    Serial.println("\n[MQTT / CLOUD] Configurando subsistema de comunicacao em nuvem...");

    WiFi.mode(WIFI_STA);

    #if MQTT_USE_TLS
      // No ESP32, setInsecure() permite comunicacao TLS com HiveMQ Cloud (Porta 8883)
      // sem exigir o armazenamento do certificado CA raiz no flash, reduzindo consumo de memoria.
      secureClient.setInsecure();
      client.setClient(secureClient);
      Serial.printf("[MQTT / CLOUD] Modo SEGURO ativado (TLS/SSL - Porta %d).\n", MQTT_PORT);
    #else
      client.setClient(standardClient);
      Serial.printf("[MQTT / CLOUD] Modo PADRAO ativado (TCP Nao Criptografado - Porta %d).\n", MQTT_PORT);
    #endif

    client.setServer(MQTT_BROKER, MQTT_PORT);
    client.setBufferSize(768); // Amplia o buffer interno do PubSubClient para suportar payloads JSON completos
    
    // Inicia primeira conexao Wi-Fi
    connectWiFi();
  }

  /**
   * Conecta a rede Wi-Fi configurada (nao bloqueante longo)
   */
  void connectWiFi() {
    if (WiFi.status() == WL_CONNECTED) {
      return;
    }

    Serial.printf("[WI-FI] Conectando a rede: %s ...\n", WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  }

  /**
   * Mantem o loop do MQTT ativo e processa reconexoes de forma nao bloqueante
   */
  void loop() {
    unsigned long now = millis();

    // 1. Verificacao da conexao Wi-Fi
    if (WiFi.status() != WL_CONNECTED) {
      if (isConnectedState) {
        Serial.println("\n[WI-FI] >>> DESCONEXAO DETECTADA! Dispositivo operando em Modo Borda Resiliente (Offline) <<<");
        isConnectedState = false;
      }
      if (now - lastWifiRetryMs >= WIFI_RETRY_INTERVAL_MS) {
        lastWifiRetryMs = now;
        connectWiFi();
      }
      return;
    }

    // 2. Se o Wi-Fi esta OK mas o MQTT caiu, tenta reconectar ao broker
    if (!client.connected()) {
      if (isConnectedState) {
        Serial.println("\n[MQTT] >>> CONEXAO COM O BROKER PERDIDA! Ativando retencao offline <<<");
        isConnectedState = false;
      }
      if (now - lastMqttRetryMs >= MQTT_RETRY_INTERVAL_MS) {
        lastMqttRetryMs = now;
        reconnectMqtt();
      }
    } else {
      client.loop();
    }
  }

  /**
   * Tenta reconectar ao HiveMQ Cloud com credenciais e Last Will and Testament (LWT)
   */
  bool reconnectMqtt() {
    Serial.printf("[MQTT] Tentando conectar ao HiveMQ Cloud (%s:%d)...\n", MQTT_BROKER, MQTT_PORT);

    // Parametros do Last Will and Testament (LWT):
    // Se o dispositivo perder a conexao subitamente, o HiveMQ avisa o Node-RED automaticamente!
    const char* willTopic = TOPIC_STATUS;
    int willQoS = 1;
    bool willRetain = true;
    const char* willMessage = LWT_PAYLOAD_OFFLINE;

    bool connected = false;
    #if MQTT_USE_TLS
      connected = client.connect(MQTT_CLIENT_ID, MQTT_USERNAME, MQTT_PASSWORD, 
                                 willTopic, willQoS, willRetain, willMessage);
    #else
      connected = client.connect(MQTT_CLIENT_ID, willTopic, willQoS, willRetain, willMessage);
    #endif

    if (connected) {
      Serial.println("[MQTT] >>> CONECTADO COM SUCESSO AO HIVEMQ CLOUD! <<<");
      isConnectedState = true;

      // Publica status ONLINE imediato
      client.publish(TOPIC_STATUS, LWT_PAYLOAD_ONLINE, true);

      // Pode assinar comandos remotos se necessario
      client.subscribe(TOPIC_COMMANDS);
      Serial.printf("[MQTT] Inscrito no topico de comandos: %s\n", TOPIC_COMMANDS);
      return true;
    } else {
      Serial.printf("[MQTT] Falha na conexao. Codigo de erro rc=%d. Nova tentativa em %ds.\n", 
                    client.state(), MQTT_RETRY_INTERVAL_MS / 1000);
      return false;
    }
  }

  bool isConnected() {
    return (WiFi.status() == WL_CONNECTED) && client.connected();
  }

  /**
   * Publica telemetria completa em formato JSON estruturado e tambem nos topicos especificos
   */
  bool publishTelemetry(const VitalsReading &reading, int offlineQueueCount, bool isSynced = false, unsigned long originalTimestamp = 0) {
    if (!isConnected()) {
      return false;
    }

    #if ARDUINOJSON_VERSION_MAJOR >= 7
      JsonDocument doc;
    #else
      StaticJsonDocument<512> doc;
    #endif

    doc["device_id"] = DEVICE_ID;
    doc["patient_id"] = PATIENT_ID;
    doc["timestamp_ms"] = (originalTimestamp > 0) ? originalTimestamp : millis();
    doc["uptime_s"] = millis() / 1000;

    JsonObject vitals = doc.createNestedObject("vitals");
    vitals["heart_rate_bpm"] = reading.bpm;
    vitals["temperature_c"] = serialized(String(reading.temperature, 1));
    vitals["humidity_pct"] = serialized(String(reading.humidity, 1));

    JsonObject status = doc.createNestedObject("status");
    status["connection"] = "ONLINE";
    status["offline_queue_size"] = offlineQueueCount;
    status["is_synced_data"] = isSynced;

    JsonObject alerts = doc.createNestedObject("alerts");
    alerts["active"] = reading.isAlert;
    alerts["type"] = reading.alertType;
    alerts["message"] = reading.alertMessage;

    char jsonBuffer[512];
    serializeJson(doc, jsonBuffer);

    // 1. Publica no topico consolidado de telemetria
    bool pubOk = client.publish(TOPIC_TELEMETRY, jsonBuffer);

    if (pubOk) {
      // 2. Publica tambem nos topicos granulares para facil manipulacao
      char numBuffer[16];
      
      itoa(reading.bpm, numBuffer, 10);
      client.publish(TOPIC_BPM, numBuffer);

      dtostrf(reading.temperature, 4, 1, numBuffer);
      client.publish(TOPIC_TEMPERATURE, numBuffer);

      dtostrf(reading.humidity, 4, 1, numBuffer);
      client.publish(TOPIC_HUMIDITY, numBuffer);

      if (reading.isAlert) {
        #if ARDUINOJSON_VERSION_MAJOR >= 7
          JsonDocument alertDoc;
        #else
          StaticJsonDocument<256> alertDoc;
        #endif
        alertDoc["patient_id"] = PATIENT_ID;
        alertDoc["alert_type"] = reading.alertType;
        alertDoc["message"] = reading.alertMessage;
        alertDoc["bpm"] = reading.bpm;
        alertDoc["temperature"] = reading.temperature;
        alertDoc["timestamp"] = (originalTimestamp > 0) ? originalTimestamp : millis();

        char alertJson[256];
        serializeJson(alertDoc, alertJson);
        client.publish(TOPIC_ALERTS, alertJson);
      }
    }

    return pubOk;
  }
};

#endif // CARDIOIA_MQTT_MANAGER_H
