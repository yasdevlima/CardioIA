/**
 * ==============================================================================
 * PROJETO CARDIOIA - A NOVA ERA DA CARDIOLOGIA INTELIGENTE (FIAP)
 * Arquivo: offline_manager.h
 * Descricao: Gerenciador de Resiliencia Offline e Armazenamento na Borda (Edge).
 *            Implementa fila circular FIFO em memoria RAM para o simulador Wokwi
 *            e suporte estruturado a SPIFFS para persistencia em hardware fisico.
 * ==============================================================================
 */

#ifndef CARDIOIA_OFFLINE_MANAGER_H
#define CARDIOIA_OFFLINE_MANAGER_H

#include <Arduino.h>
#include <SPIFFS.h>
#include "config.h"
#include "sensors.h"

// Estrutura de registro offline enriquecida com metadados de auditoria
struct OfflineTelemetryRecord {
  VitalsReading vitals;
  unsigned long recordedAtMs;
  uint32_t sequenceNumber;
  bool isAlert;
};

class OfflineManager {
private:
  // Fila circular em memoria RAM (estatica para evitar fragmentacao de Heap)
  OfflineTelemetryRecord queue[MAX_OFFLINE_RECORDS];
  int head; // Aponta para a posicao do proximo item a ser inserido
  int tail; // Aponta para o item mais antigo a ser desenfileirado
  int count; // Numero atual de itens pendentes
  uint32_t currentSequence;
  
  bool spiffsAvailable;
  uint32_t totalDroppedRecords; // Contador de registros descartados por transbordo

public:
  OfflineManager() : head(0), tail(0), count(0), currentSequence(0), 
                     spiffsAvailable(false), totalDroppedRecords(0) {}

  /**
   * Inicializa o subsistema de armazenamento local e resiliencia.
   * Tenta montar a particao SPIFFS do ESP32 para hardware real.
   */
  void begin() {
    Serial.println("\n------------------------------------------------------------");
    Serial.println("[OFFLINE RESILIENCE] Inicializando gerenciador de borda...");

    // Inicializacao do SPIFFS com formatacao automatica caso nao esteja montado
    if (SPIFFS.begin(true)) {
      spiffsAvailable = true;
      size_t totalBytes = SPIFFS.totalBytes();
      size_t usedBytes = SPIFFS.usedBytes();
      Serial.printf("[SPIFFS] Montado com sucesso! Espaco Total: %u bytes | Usado: %u bytes\n", 
                    totalBytes, usedBytes);
      
      // Tenta recuperar registros pendentes de uma reinicializacao previa
      recoverFromSPIFFS();
    } else {
      spiffsAvailable = false;
      Serial.println("[SPIFFS] Aviso: Falha ao montar SPIFFS (comum no simulador Wokwi).");
      Serial.println("[OFFLINE RESILIENCE] Ativando modo de Fila Circular em Memoria RAM (Fallback resiliente).");
    }

    Serial.printf("[OFFLINE RESILIENCE] Fila em RAM configurada para ate %d registros.\n", MAX_OFFLINE_RECORDS);
    Serial.println("------------------------------------------------------------\n");
  }

  /**
   * Armazena uma leitura na fila quando o dispositivo esta offline.
   * Implementa politica de memoria finita (evita overflow de heap e flash).
   */
  bool enqueue(const VitalsReading &reading) {
    currentSequence++;

    OfflineTelemetryRecord record;
    record.vitals = reading;
    record.recordedAtMs = millis();
    record.sequenceNumber = currentSequence;
    record.isAlert = reading.isAlert;

    // Se a fila estiver cheia, sobrescreve o mais antigo (politica circular)
    // para garantir que os dados mais recentes do paciente nunca sejam perdidos.
    if (isFull()) {
      Serial.println("[OFFLINE RESILIENCE] ALERTA: Fila cheia! Descartando registro mais antigo (Politica FIFO Circular).");
      tail = (tail + 1) % MAX_OFFLINE_RECORDS;
      count--;
      totalDroppedRecords++;
    }

    queue[head] = record;
    head = (head + 1) % MAX_OFFLINE_RECORDS;
    count++;

    Serial.printf("[OFFLINE RESILIENCE] Registro #%u retido localmente. Fila: %d/%d pendentes. (BPM: %d | Temp: %.1f C | Alerta: %s)\n",
                  record.sequenceNumber, count, MAX_OFFLINE_RECORDS, 
                  record.vitals.bpm, record.vitals.temperature, 
                  record.isAlert ? "SIM" : "NAO");

    // Espelha no SPIFFS caso hardware real esteja disponivel
    if (spiffsAvailable) {
      appendRecordToSPIFFS(record);
    }

    return true;
  }

  /**
   * Consulta o registro mais antigo da fila SEM remove-lo.
   * Garante que o dado so sera descartado apos confirmacao de entrega MQTT (Zero Data Loss).
   */
  bool peek(OfflineTelemetryRecord &record) {
    if (isEmpty()) {
      return false;
    }
    record = queue[tail];
    return true;
  }

  /**
   * Remove definitivamente o registro mais antigo da fila.
   * Deve ser chamado APENAS apos confirmacao de envio com sucesso pelo broker MQTT.
   */
  bool confirmTransmission() {
    if (isEmpty()) {
      return false;
    }
    
    uint32_t removedSeq = queue[tail].sequenceNumber;
    tail = (tail + 1) % MAX_OFFLINE_RECORDS;
    count--;

    Serial.printf("[OFFLINE RESILIENCE] Confirmado envio do registro #%u com sucesso. Restam %d pendentes na fila.\n", 
                  removedSeq, count);

    // Se a fila esvaziou, limpa o arquivo SPIFFS de contingencia
    if (count == 0 && spiffsAvailable) {
      clearSPIFFSFile();
    }

    return true;
  }

  int getCount() const { return count; }
  bool isEmpty() const { return count == 0; }
  bool isFull() const { return count >= MAX_OFFLINE_RECORDS; }
  uint32_t getDroppedCount() const { return totalDroppedRecords; }

private:
  /**
   * Salva o registro em arquivo binario sequencial na memoria flash SPIFFS
   */
  void appendRecordToSPIFFS(const OfflineTelemetryRecord &record) {
    File file = SPIFFS.open(SPIFFS_FILE_PATH, FILE_APPEND);
    if (!file) {
      Serial.println("[SPIFFS] Erro ao abrir arquivo para gravacao offline.");
      return;
    }
    file.write((const uint8_t*)&record, sizeof(OfflineTelemetryRecord));
    file.close();
  }

  /**
   * Limpa o arquivo SPIFFS apos sincronizacao concluida
   */
  void clearSPIFFSFile() {
    if (SPIFFS.exists(SPIFFS_FILE_PATH)) {
      SPIFFS.remove(SPIFFS_FILE_PATH);
      Serial.println("[SPIFFS] Arquivo de dados pendentes limpo apos sincronizacao completa.");
    }
  }

  /**
   * Carrega registros pendentes gravados na flash antes de um reinicio
   */
  void recoverFromSPIFFS() {
    if (!SPIFFS.exists(SPIFFS_FILE_PATH)) {
      return;
    }

    File file = SPIFFS.open(SPIFFS_FILE_PATH, FILE_READ);
    if (!file) {
      return;
    }

    size_t recordSize = sizeof(OfflineTelemetryRecord);
    int recovered = 0;

    while (file.available() >= recordSize && !isFull()) {
      OfflineTelemetryRecord rec;
      file.read((uint8_t*)&rec, recordSize);
      
      queue[head] = rec;
      head = (head + 1) % MAX_OFFLINE_RECORDS;
      count++;
      recovered++;
    }
    file.close();

    if (recovered > 0) {
      Serial.printf("[SPIFFS] %d registros offline recuperados da memoria flash com sucesso!\n", recovered);
    }
  }
};

#endif // CARDIOIA_OFFLINE_MANAGER_H
