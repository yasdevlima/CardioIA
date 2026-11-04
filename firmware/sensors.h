/**
 * ==============================================================================
 * PROJETO CARDIOIA - A NOVA ERA DA CARDIOLOGIA INTELIGENTE (FIAP)
 * Arquivo: sensors.h
 * Descricao: Gerenciador e processamento de sinais dos sensores vitais na borda.
 * ==============================================================================
 */

#ifndef CARDIOIA_SENSORS_H
#define CARDIOIA_SENSORS_H

#include <Arduino.h>
#include <DHT.h>
#include "config.h"

// Estrutura completa para encapsular uma medicao dos sinais vitais
struct VitalsReading {
  int bpm;
  float temperature;
  float humidity;
  bool isAlert;
  char alertType[24];     // "NORMAL", "TAQUICARDIA", "BRADICARDIA", "FEBRE", etc.
  char alertMessage[64];  // Descritivo clinico para exibicao no prontuario
  unsigned long timestamp;
};

class SensorsManager {
private:
  DHT dht;
  
  // Filtro de media movel para estabilizacao do sinal do ADC (Potenciometro)
  static const int FILTER_WINDOW_SIZE = 5;
  int adcBuffer[FILTER_WINDOW_SIZE];
  int filterIndex;
  bool filterFilled;

public:
  SensorsManager() : dht(PIN_DHT, DHT22), filterIndex(0), filterFilled(false) {
    for (int i = 0; i < FILTER_WINDOW_SIZE; i++) {
      adcBuffer[i] = 2048; // Inicializa com valor intermediario (~110 BPM)
    }
  }

  void begin() {
    dht.begin();
    pinMode(PIN_POT_BPM, INPUT);
    analogReadResolution(12); // ESP32 possui ADC de 12 bits (0 a 4095)
    
    // Configuracao de atenuacao para leitura linear completa ate 3.3V
    #if defined(ESP32)
      analogSetPinAttenuation(PIN_POT_BPM, ADC_11db);
    #endif

    Serial.println("[SENSORES] DHT22 e Conversor ADC de Frequencia Cardiaca inicializados com sucesso.");
  }

  /**
   * Converte a leitura bruta do potenciometro (0 - 4095) na faixa fisiologica (40 - 180 BPM)
   * Aplicando filtro de media movel para reduzir ruidos analogicos da simulacao/hardware.
   */
  int readFilteredBPM() {
    int rawAdc = analogRead(PIN_POT_BPM);
    
    // Atualiza buffer circular da media movel
    adcBuffer[filterIndex] = rawAdc;
    filterIndex = (filterIndex + 1) % FILTER_WINDOW_SIZE;
    if (filterIndex == 0) filterFilled = true;

    int sum = 0;
    int count = filterFilled ? FILTER_WINDOW_SIZE : filterIndex;
    if (count == 0) count = 1;

    for (int i = 0; i < count; i++) {
      sum += adcBuffer[i];
    }
    int averageAdc = sum / count;

    // Mapeamento linear: 0 a 4095 -> 40 a 180 BPM
    int bpm = map(averageAdc, 0, 4095, BPM_MIN_CRITICAL, BPM_MAX_CRITICAL);
    return constrain(bpm, BPM_MIN_CRITICAL, BPM_MAX_CRITICAL);
  }

  /**
   * Realiza a leitura e processamento inteligente na borda (Edge Computing)
   */
  VitalsReading readAll() {
    VitalsReading reading;
    reading.timestamp = millis();
    
    // 1. Leitura da frequencia cardiaca simulada
    reading.bpm = readFilteredBPM();

    // 2. Leitura de Temperatura e Umidade corporal simulada
    float t = dht.readTemperature();
    float h = dht.readHumidity();

    // Tratamento de falhas de leitura do DHT22 com valores de contingencia
    if (isnan(t)) {
      reading.temperature = 36.5f; // Valor basal de seguranca
      Serial.println("[SENSORES] Alerta: Falha na leitura do DHT22. Utilizando temperatura basal de contingencia.");
    } else {
      reading.temperature = t;
    }

    if (isnan(h)) {
      reading.humidity = 50.0f;
    } else {
      reading.humidity = h;
    }

    // 3. Processamento Local na Borda (Edge Analytics): Classificacao Clinica e Deteccao de Anomalias
    evaluateClinicalCondition(reading);

    return reading;
  }

private:
  /**
   * Analise algoritmica na borda dos parametros clinicos
   */
  void evaluateClinicalCondition(VitalsReading &r) {
    r.isAlert = false;
    strncpy(r.alertType, "NORMAL", sizeof(r.alertType));
    strncpy(r.alertMessage, "Parametros estaveis dentro do padrao clinico.", sizeof(r.alertMessage));

    // Regra 1: Taquicardia severa (Criterio prioritario do projeto > 120 BPM)
    if (r.bpm >= BPM_ALERT_THRESHOLD) {
      r.isAlert = true;
      strncpy(r.alertType, "TAQUICARDIA_GRAVE", sizeof(r.alertType));
      snprintf(r.alertMessage, sizeof(r.alertMessage), "FC Critica: %d BPM. Risco iminente de colapso hemodinamico!", r.bpm);
      return;
    }

    // Regra 2: Febre / Hipertermia (Criterio prioritario do projeto > 38.0 °C)
    if (r.temperature >= TEMP_ALERT_THRESHOLD) {
      r.isAlert = true;
      strncpy(r.alertType, "HIPERTERMIA_FEBRE", sizeof(r.alertType));
      snprintf(r.alertMessage, sizeof(r.alertMessage), "Temperatura Critica: %.1f C. Estado febril detectado!", r.temperature);
      return;
    }

    // Regra 3: Bradicardia severa
    if (r.bpm < BPM_MIN_NORMAL) {
      r.isAlert = true;
      strncpy(r.alertType, "BRADICARDIA", sizeof(r.alertType));
      snprintf(r.alertMessage, sizeof(r.alertMessage), "FC Baixa: %d BPM. Alerta de hipoperfusao.", r.bpm);
      return;
    }

    // Regra 4: Hipotermia
    if (r.temperature < TEMP_NORMAL_MIN) {
      r.isAlert = true;
      strncpy(r.alertType, "HIPOTERMIA", sizeof(r.alertType));
      snprintf(r.alertMessage, sizeof(r.alertMessage), "Temperatura Baixa: %.1f C. Alerta de hipotermia corporal.", r.temperature);
      return;
    }

    // Regra 5: Taquicardia leve a moderada (Aviso)
    if (r.bpm > BPM_MAX_NORMAL) {
      strncpy(r.alertType, "TAQUICARDIA_LEVE", sizeof(r.alertType));
      snprintf(r.alertMessage, sizeof(r.alertMessage), "FC Elevada: %d BPM. Em observacao.", r.bpm);
    }
  }
};

#endif // CARDIOIA_SENSORS_H
