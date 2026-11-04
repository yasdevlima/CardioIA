#!/usr/bin/env python3
"""
==============================================================================
PROJETO CARDIOIA - A NOVA ERA DA CARDIOLOGIA INTELIGENTE (FIAP)
Script: mock_publisher.py
Descricao: Simulador de dispositivo vestivel ESP32 para teste de estresse,
           validacao de dashboard e demonstracao do fluxo offline/sincronizado.
==============================================================================
"""

import sys
import json
import time
from datetime import datetime

try:
    import paho.mqtt.client as mqtt
except ImportError:
    print("[ERRO] Biblioteca 'paho-mqtt' nao encontrada. Instale com: pip install paho-mqtt")
    sys.exit(1)

BROKER_HOST = "broker.hivemq.com"
BROKER_PORT = 1883
TOPIC_BASE = "cardioia/paciente01"

def create_telemetry_payload(bpm, temp, hum, is_synced=False, queue_size=0, timestamp_ms=None):
    is_alert = (bpm >= 120) or (temp >= 38.0) or (bpm < 60)
    
    alert_type = "NORMAL"
    alert_msg = "Sinais vitais normais"
    if bpm >= 120:
        alert_type = "TAQUICARDIA_GRAVE"
        alert_msg = f"FC Critica: {bpm} BPM. Risco iminente de colapso!"
    elif temp >= 38.0:
        alert_type = "HIPERTERMIA_FEBRE"
        alert_msg = f"Temperatura Critica: {temp:.1f} °C. Estado febril!"
    elif bpm < 60:
        alert_type = "BRADICARDIA"
        alert_msg = f"FC Baixa: {bpm} BPM. Alerta de hipoperfusao."

    now_ms = timestamp_ms if timestamp_ms else int(time.time() * 1000)

    return {
        "device_id": "MOCK-ESP32-SIM",
        "patient_id": "paciente01",
        "timestamp_ms": now_ms,
        "uptime_s": int(time.time()),
        "vitals": {
            "heart_rate_bpm": bpm,
            "temperature_c": f"{temp:.1f}",
            "humidity_pct": f"{hum:.1f}"
        },
        "status": {
            "connection": "ONLINE",
            "offline_queue_size": queue_size,
            "is_synced_data": is_synced
        },
        "alerts": {
            "active": is_alert,
            "type": alert_type,
            "message": alert_msg
        }
    }

def main():
    print("=============================================================")
    print(" SIMULADOR DE FLUXO E TESTE DE ESTRESSE - CARDIOIA ")
    print("=============================================================")

    client = mqtt.Client("CardioIA_Mock_ESP32")
    print(f"Conectando ao broker {BROKER_HOST}:{BROKER_PORT}...")
    client.connect(BROKER_HOST, BROKER_PORT, 60)
    client.loop_start()

    time.sleep(1)
    print("\n[FASE 1] Enviando 5 leituras normais em tempo real (75 BPM, 36.6 °C)...")
    for i in range(5):
        payload = create_telemetry_payload(bpm=72 + i, temp=36.5 + (i * 0.1), hum=52.0)
        client.publish(f"{TOPIC_BASE}/telemetria", json.dumps(payload))
        client.publish(f"{TOPIC_BASE}/bpm", str(payload["vitals"]["heart_rate_bpm"]))
        client.publish(f"{TOPIC_BASE}/temperatura", str(payload["vitals"]["temperature_c"]))
        print(f" -> Telemetria enviada: {payload['vitals']['heart_rate_bpm']} BPM | {payload['vitals']['temperature_c']} °C")
        time.sleep(2)

    print("\n[FASE 2] Simulando APAGAO DE REDE / MODO OFFLINE (Acumulando leituras na fila local)...")
    offline_buffer = []
    simulated_bpms = [80, 95, 110, 128, 135] # Note: reaching alert state while offline!
    base_time = int(time.time() * 1000)

    for idx, bpm in enumerate(simulated_bpms):
        rec_time = base_time + (idx * 3000)
        temp = 36.8 + (idx * 0.3)
        payload = create_telemetry_payload(bpm=bpm, temp=temp, hum=50.0, is_synced=True, queue_size=len(simulated_bpms) - idx, timestamp_ms=rec_time)
        offline_buffer.append(payload)
        print(f" -> [RETENCAO OFFLINE #{idx+1}] Gravado em buffer: {bpm} BPM | {temp:.1f} °C (Alerta: {payload['alerts']['active']})")
        time.sleep(1)

    print("\n[FASE 3] CONEXAO RESTAURADA! Descarregando fila offline para a nuvem em FIFO...")
    for idx, payload in enumerate(offline_buffer):
        client.publish(f"{TOPIC_BASE}/telemetria", json.dumps(payload))
        client.publish(f"{TOPIC_BASE}/bpm", str(payload["vitals"]["heart_rate_bpm"]))
        client.publish(f"{TOPIC_BASE}/temperatura", str(payload["vitals"]["temperature_c"]))
        if payload["alerts"]["active"]:
            client.publish(f"{TOPIC_BASE}/alertas", json.dumps(payload["alerts"]))
        print(f" -> [SINCRONIZADO #{idx+1}] Confirmado envio de registro offline: {payload['vitals']['heart_rate_bpm']} BPM (Timestamp: {payload['timestamp_ms']})")
        time.sleep(0.3)

    print("\n[FASE 4] Teste de Alerta Clinico Critico (Taquicardia 145 BPM)...")
    alert_payload = create_telemetry_payload(bpm=145, temp=38.6, hum=48.0)
    client.publish(f"{TOPIC_BASE}/telemetria", json.dumps(alert_payload))
    client.publish(f"{TOPIC_BASE}/bpm", "145")
    client.publish(f"{TOPIC_BASE}/temperatura", "38.6")
    client.publish(f"{TOPIC_BASE}/alertas", json.dumps(alert_payload["alerts"]))
    print(f" -> Alerta disparado com sucesso no Node-RED e HiveMQ!")

    time.sleep(2)
    client.loop_stop()
    client.disconnect()
    print("\nTeste concluido com sucesso!")

if __name__ == "__main__":
    main()
