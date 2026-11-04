#!/usr/bin/env python3
"""
==============================================================================
PROJETO CARDIOIA - A NOVA ERA DA CARDIOLOGIA INTELIGENTE (FIAP)
Script: cardioia_subscriber.py
Descricao: Cliente Python de auditoria, validacao e recepcao de telemetria MQTT
           diretamente do HiveMQ Cloud ou broker HiveMQ.
==============================================================================
"""

import sys
import json
import time
from datetime import datetime

try:
    import paho.mqtt.client as mqtt
except ImportError:
    print("[ERRO] Biblioteca 'paho-mqtt' nao encontrada.")
    print("Execute: pip install -r requirements.txt")
    sys.exit(1)

# ==============================================================================
# CONFIGURACOES DE CONEXAO
# ==============================================================================
# Ajuste conforme suas configuracoes em firmware/config.h
USE_TLS = False  # True para HiveMQ Cloud (porta 8883), False para broker publico (1883)

if USE_TLS:
    BROKER_HOST = "SEU_CLUSTER_HIVEMQ.s1.eu.hivemq.cloud"
    BROKER_PORT = 8883
    BROKER_USER = "cardioia_user"
    BROKER_PASS = "CardioIA@2026"
else:
    BROKER_HOST = "broker.hivemq.com"
    BROKER_PORT = 1883
    BROKER_USER = None
    BROKER_PASS = None

TOPIC_SUB = "cardioia/paciente01/#"

# Metricas acumuladas
stats = {
    "total_messages": 0,
    "telemetry_count": 0,
    "alerts_count": 0,
    "synced_count": 0,
    "bpm_history": []
}

def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print("\n" + "=" * 65)
        print(f" [CARDIOIA - MONITOR NUVEM] Conectado ao Broker MQTT com Sucesso!")
        print(f" Host: {BROKER_HOST}:{BROKER_PORT} | Topico: {TOPIC_SUB}")
        print("=" * 65 + "\n")
        client.subscribe(TOPIC_SUB)
    else:
        print(f"[ERRO] Falha ao conectar ao broker MQTT. Codigo de retorno: {rc}")

def on_message(client, userdata, msg):
    stats["total_messages"] += 1
    timestamp_local = datetime.now().strftime("%H:%M:%S")

    try:
        topic = msg.topic
        payload_str = msg.payload.decode("utf-8")

        # Telemetria consolidada em JSON
        if topic.endswith("/telemetria"):
            stats["telemetry_count"] += 1
            data = json.loads(payload_str)
            
            vitals = data.get("vitals", {})
            bpm = vitals.get("heart_rate_bpm", 0)
            temp = float(vitals.get("temperature_c", 0.0))
            hum = float(vitals.get("humidity_pct", 0.0))
            
            status = data.get("status", {})
            is_synced = status.get("is_synced_data", False)
            queue_len = status.get("offline_queue_size", 0)

            alerts = data.get("alerts", {})
            is_alert = alerts.get("active", False)
            alert_type = alerts.get("type", "NORMAL")

            if is_synced:
                stats["synced_count"] += 1
            if is_alert:
                stats["alerts_count"] += 1
            if bpm > 0:
                stats["bpm_history"].append(bpm)

            sync_flag = " [SINCRONIZADO OFFLINE]" if is_synced else " [TEMPO REAL]"
            alert_flag = f" >>> ALERTA: {alert_type} <<<" if is_alert else ""

            print(f"[{timestamp_local}] TELEMETRIA{sync_flag}{alert_flag}")
            print(f"   | Paciente: {data.get('patient_id')} | FC: {bpm} BPM | Temp: {temp:.1f} °C | Umid: {hum:.1f} %")
            print(f"   | Fila Residual: {queue_len} | Diagnostico: {alerts.get('message', 'Normal')}")
            print("-" * 65)

        elif topic.endswith("/alertas"):
            print(f"\n[{timestamp_local}] !!! ALERTA MEDICO CRITICO NO TOPICO ESPECIFICO !!!")
            print(f"   | Topico: {topic}")
            print(f"   | Payload: {payload_str}")
            print("!" * 65 + "\n")

        elif topic.endswith("/status"):
            print(f"[{timestamp_local}] [STATUS DISPOSITIVO] {payload_str}")

    except Exception as e:
        print(f"[{timestamp_local}] [{msg.topic}] {msg.payload.decode('utf-8', errors='ignore')} (Erro parse: {e})")

def main():
    print("=============================================================")
    print(" CARDIOIA - SUBSCRITOR DE TELEMETRIA E AUDITORIA CLINICA ")
    print(" FIAP - Edge Computing & Internet of Things")
    print("=============================================================")

    client = mqtt.Client(client_id="CardioIA_Python_Auditor")
    
    if USE_TLS:
        client.tls_set()
        if BROKER_USER and BROKER_PASS:
            client.username_pw_set(BROKER_USER, BROKER_PASS)

    client.on_connect = on_connect
    client.on_message = on_message

    try:
        print(f"Conectando a {BROKER_HOST}:{BROKER_PORT}...")
        client.connect(BROKER_HOST, BROKER_PORT, 60)
        client.loop_forever()
    except KeyboardInterrupt:
        print("\n\nEncerrando monitoramento CardioIA.")
        print("=" * 65)
        print(" RESUMO DA SESSAO DE MONITORAMENTO:")
        print(f" - Total de mensagens recebidas: {stats['total_messages']}")
        print(f" - Pacotes de telemetria:        {stats['telemetry_count']}")
        print(f" - Pacotes sincronizados offline:{stats['synced_count']}")
        print(f" - Ocorrencias de alerta:        {stats['alerts_count']}")
        if stats["bpm_history"]:
            avg_bpm = sum(stats["bpm_history"]) / len(stats["bpm_history"])
            print(f" - Frequencia Cardiaca Media:    {avg_bpm:.1f} BPM")
            print(f" - Frequencia Cardiaca Maxima:   {max(stats['bpm_history'])} BPM")
            print(f" - Frequencia Cardiaca Minima:   {min(stats['bpm_history'])} BPM")
        print("=" * 65)
        client.disconnect()

if __name__ == "__main__":
    main()
