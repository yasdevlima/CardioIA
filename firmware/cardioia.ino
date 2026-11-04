/**
 * ==============================================================================
 * PROJETO CARDIOIA – A NOVA ERA DA CARDIOLOGIA INTELIGENTE
 * Atividade Academica FIAP - Internet of Things & Edge Computing
 * 
 * Descricao: Firmware principal do dispositivo vestivel inteligente de
 *            monitoramento cardiaco baseado no microcontrolador ESP32.
 * 
 * Funcionalidades:
 *  - Edge Computing: Captura e analise clinica local de sinais vitais (BPM, Temp, Umidade).
 *  - Deteccao de Anomalias na Borda: Disparo imediato de alarmes para FC > 120 BPM ou Temp > 38C.
 *  - Resiliencia Offline: Armazenamento em fila FIFO e SPIFFS com sincronizacao automatica.
 *  - Cloud IoT & MQTT: Integracao com HiveMQ Cloud e transmissao de telemetria JSON.
 * ==============================================================================
 */

#include <Arduino.h>
#include "config.h"
#include "sensors.h"
#include "offline_manager.h"
#include "mqtt_manager.h"

// Instancias dos modulos do sistema
SensorsManager sensors;
OfflineManager offlineStorage;
MqttManager mqttCloud;

// Variaveis de controle temporal (Multitarefa cooperativa baseada em millis)
unsigned long lastSensorSampleTime = 0;
unsigned long lastSyncStepTime = 0;
bool wasOfflinePreviously = false;

// Prototipos de funcoes locais
void handleLocalActuators(const VitalsReading &reading);
void printTelemetryLog(const VitalsReading &reading, bool isConnected);
void processOfflineSynchronization();

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n============================================================");
  Serial.println("   CARDIOIA - SISTEMA INTELIGENTE DE MONITORAMENTO CARDIACO ");
  Serial.println("        FIAP - Engenharia de Software / Pos-Graduacao       ");
  Serial.println("============================================================");
  Serial.printf("Dispositivo: %s | Paciente: %s\n", DEVICE_ID, PATIENT_ID);
  Serial.println("Inicializando componentes de hardware e firmware...");

  // Configuracao dos pinos de atuadores locais
  pinMode(PIN_LED_STATUS, OUTPUT);
  pinMode(PIN_LED_ALERT, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);

  digitalWrite(PIN_LED_STATUS, LOW);
  digitalWrite(PIN_LED_ALERT, LOW);
  digitalWrite(PIN_BUZZER, LOW);

  // Inicializacao dos sensores (DHT22 e ADC Potenciometro)
  sensors.begin();

  // Inicializacao do subsistema de armazenamento local (RAM + SPIFFS)
  offlineStorage.begin();

  // Inicializacao da camada de rede e comunicacao MQTT
  mqttCloud.begin();

  Serial.println("[SISTEMA] Inicializacao concluida. Monitoramento cardiaco ativo!");
  Serial.println("============================================================\n");
}

void loop() {
  unsigned long currentMillis = millis();

  // 1. Manutencao da pilha de comunicacao Wi-Fi e MQTT
  mqttCloud.loop();

  // 2. Deteccao de transicao de estado (Reconexao a Nuvem)
  bool isConnected = mqttCloud.isConnected();
  if (isConnected && wasOfflinePreviously) {
    Serial.println("\n************************************************************");
    Serial.printf("[CONEXAO RESTAURADA] Reconectado a nuvem! %d registros pendentes para sincronizar.\n", 
                  offlineStorage.getCount());
    Serial.println("************************************************************\n");
    wasOfflinePreviously = false;
  } else if (!isConnected && !wasOfflinePreviously) {
    wasOfflinePreviously = true;
  }

  // 3. Processamento continuo de sincronizacao de dados retidos offline (sem travar o loop)
  if (isConnected && !offlineStorage.isEmpty()) {
    if (currentMillis - lastSyncStepTime >= SYNC_INTERVAL_MS) {
      lastSyncStepTime = currentMillis;
      processOfflineSynchronization();
    }
  }

  // 4. Ciclo periodico de leitura e processamento local dos sensores (Edge Computing)
  if (currentMillis - lastSensorSampleTime >= SENSOR_READ_INTERVAL_MS) {
    lastSensorSampleTime = currentMillis;

    // A. Captura e avaliacao algoritmica na borda
    VitalsReading reading = sensors.readAll();

    // B. Atuacao fisica imediata na borda (Zero Latency Edge Response)
    handleLocalActuators(reading);

    // C. Impressao de diagnostico no Monitor Serial
    printTelemetryLog(reading, isConnected);

    // D. Decisao de roteamento: Nuvem Direta vs Fila Local Resiliente
    if (isConnected) {
      // Se houver itens offline anteriores, eles tem prioridade (FIFO).
      // Caso a fila esteja vazia, envia a leitura atual em tempo real.
      if (offlineStorage.isEmpty()) {
        bool sent = mqttCloud.publishTelemetry(reading, 0, false);
        if (sent) {
          Serial.println("[TELEMETRIA] Leitura transmitida em tempo real para o HiveMQ Cloud.");
        } else {
          Serial.println("[TELEMETRIA] Falha no envio imediato. Armazenando na fila de contingencia.");
          offlineStorage.enqueue(reading);
        }
      } else {
        // Enfileira para preservar ordem cronologica estrita
        offlineStorage.enqueue(reading);
      }
    } else {
      // Dispositivo offline: armazena na memoria local protegida
      Serial.println("[MODO OFFLINE] Sem conexao ativa. Gravando leitura no buffer local resiliente.");
      offlineStorage.enqueue(reading);
    }
  }
}

/**
 * Controle de atuadores na borda (Edge Computing):
 * Dispara alertas visuais e sonoros instantaneamente em caso de anomalia clinica.
 */
void handleLocalActuators(const VitalsReading &reading) {
  if (reading.isAlert) {
    // Estado de Alerta: Liga LED Vermelho e ativa alarme sonoro
    digitalWrite(PIN_LED_ALERT, HIGH);
    digitalWrite(PIN_LED_STATUS, LOW);

    // Emissao de tom sonoro pulsado para atencao medica
    tone(PIN_BUZZER, 1800, 200); // 1.8 kHz por 200ms
  } else {
    // Estado Normal: Apaga LED Vermelho e desliga Buzzer
    digitalWrite(PIN_LED_ALERT, LOW);
    noTone(PIN_BUZZER);
    digitalWrite(PIN_BUZZER, LOW);

    // Pisca rapidamente o LED Verde indicando batimento/operacao saudavel
    digitalWrite(PIN_LED_STATUS, HIGH);
    delay(50);
    digitalWrite(PIN_LED_STATUS, LOW);
  }
}

/**
 * Sincronizacao automatica dos dados armazenados durante o periodo offline
 */
void processOfflineSynchronization() {
  OfflineTelemetryRecord record;
  if (!offlineStorage.peek(record)) {
    return; // Fila vazia
  }

  Serial.printf("[SINCRONIZACAO] Transmitindo registro pendente #%u (Gravado ha %lu ms atras)...\n",
                record.sequenceNumber, millis() - record.recordedAtMs);

  // Envia com a flag is_synced = true e timestamp original da medicao
  bool published = mqttCloud.publishTelemetry(record.vitals, 
                                             offlineStorage.getCount() - 1, 
                                             true, 
                                             record.recordedAtMs);

  if (published) {
    // So deleta apos confirmacao do broker MQTT (Garantia de nao perda de dados)
    offlineStorage.confirmTransmission();
  } else {
    Serial.println("[SINCRONIZACAO] Broker ocupado ou falha temporaria. Manteremos o dado na fila.");
  }
}

/**
 * Exibicao formatada dos parametros vitais no Monitor Serial
 */
void printTelemetryLog(const VitalsReading &reading, bool isConnected) {
  Serial.println("\n----------------- [CARDIOIA - TELEMETRIA] -----------------");
  Serial.printf("Tempo de Atividade: %lu s | Status de Conexao: %s\n", 
                millis() / 1000, isConnected ? "ONLINE (Nuvem)" : "OFFLINE (Borda)");
  Serial.printf("Frequencia Cardiaca: %d BPM %s\n", 
                reading.bpm, reading.bpm >= BPM_ALERT_THRESHOLD ? ">>> [ALERTA TAQUICARDIA] <<<" : "");
  Serial.printf("Temperatura Corporal: %.1f C %s\n", 
                reading.temperature, reading.temperature >= TEMP_ALERT_THRESHOLD ? ">>> [ALERTA FEBRE] <<<" : "");
  Serial.printf("Umidade Ambiente:    %.1f %%\n", reading.humidity);
  Serial.printf("Classificacao Borda: %s\n", reading.alertType);
  Serial.printf("Diagnostico Clinico: %s\n", reading.alertMessage);
  Serial.printf("Fila Offline em RAM: %d / %d registros retidos\n", 
                offlineStorage.getCount(), MAX_OFFLINE_RECORDS);
  Serial.println("-----------------------------------------------------------\n");
}
