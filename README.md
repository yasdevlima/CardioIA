# FIAP - Faculdade de Informática e Administração Paulista

<p align="center">
    <a href= "https://www.fiap.com.br/"><img src="assets/logo-fiap.png" alt="FIAP - Faculdade de Informática e Admnistração Paulista" border="0" width=40% height=40%></a>
</p>

<br>

# CARDIOIA – A NOVA ERA DA CARDIOLOGIA INTELIGENTE
### Sistema Inteligente de Monitoramento Cardíaco com IoT, Edge Computing e Nuvem

## 👨‍🎓 Integrantes: 

- Marina Clara Constantino Ribeiro - RM568576
- Yasmin Kauane Silva Lima - RM566645



---

## 1. Visão Geral do Projeto

O **CardioIA** é um protótipo completo e funcional de monitoramento contínuo de pacientes cardiológicos internados ou em cuidados intensivos. O sistema simula um dispositivo vestível (*wearable*) inteligente baseado no microcontrolador **ESP32**, cobrindo o fluxo completo de dados biomédicos:

$$\text{Captura} \longrightarrow \text{Processamento na Borda (Edge)} \longrightarrow \text{Armazenamento Resiliente} \longrightarrow \text{Comunicação MQTT} \longrightarrow \text{Nuvem (HiveMQ)} \longrightarrow \text{Dashboard (Node-RED)} \longrightarrow \text{Alertas}$$

### Destaques Técnicos da Solução:
- **Edge Computing & Resposta Instantânea:** Detecção de taquicardia (>120 BPM) ou febre (>38 °C) com atuação física local em menos de 10 ms (LED Vermelho e Buzzer sonoro).
- **Resiliência Offline (Zero Data Loss):** Fila circular em memória RAM e partição SPIFFS retêm as medições em caso de queda de rede, sincronizando os dados automaticamente em ordem cronológica estrita (FIFO) assim que a conexão é restaurada.
- **Transmissão Segura na Nuvem:** Conexão criptografada via **TLS/SSL (Porta 8883)** ao **HiveMQ Cloud**, com tópicos estruturados em JSON e proteção contra falhas abruptas via *Last Will and Testament* (LWT).
- **Dashboard Clínico de Alta Fidelidade:** Interface moderna no **Node-RED** desenvolvida com paleta visual hospitalar (azul-marinho, azul-celeste, verde estável, âmbar e vermelho de emergência), contendo prontuário do leito, gauges dinâmicos, gráfico temporal e watchdog contra perda de sinal.

---

## 2. Estrutura do Repositório

O projeto está organizado segundo as melhores práticas de engenharia de software embarcado e IoT:

```text
CardioIA/
├── firmware/                  # Código-fonte C++ para o microcontrolador ESP32
│   ├── cardioia.ino          # Sketch principal com máquina de estados cooperativa
│   ├── config.h              # Arquivo de configuração de Wi-Fi, MQTT e limiares
│   ├── config.example.h      # Template seguro sem credenciais expostas
│   ├── sensors.h             # Leitura do DHT22, filtro ADC do BPM e análise clínica
│   ├── offline_manager.h     # Fila circular FIFO em RAM e persistência SPIFFS
│   └── mqtt_manager.h        # Gerenciamento Wi-Fi, TLS e mensageria HiveMQ
│
├── wokwi/                     # Arquivos do simulador virtual de hardware
│   ├── diagram.json          # Diagrama visual com ESP32, DHT22, Potenciômetro, LEDs e Buzzer
│   ├── libraries.txt         # Lista de dependências de bibliotecas C++ no Wokwi
│   └── wokwi.toml            # Configuração de build do simulador
│
├── node-red/                  # Camada de Fog Computing e Dashboard Hospitalar
│   ├── flows.json            # Fluxo completo exportado pronto para importação
│   └── package.json          # Dependência do node-red-dashboard
│
├── python/                    # Utilitários de teste, auditoria e simulação de carga
│   ├── cardioia_subscriber.py# Subscritor MQTT de auditoria clínica via terminal
│   ├── mock_publisher.py     # Simulador de carga e injeção de dados offline
│   └── requirements.txt      # Dependência do paho-mqtt
│
├── docs/                      # Relatórios acadêmicos e documentação formal (FIAP)
│   ├── arquitetura.md        # Diagramas e descrição detalhada da arquitetura em 4 camadas
│   ├── configuracao.md       # Guia passo a passo de setup (HiveMQ Cloud, Wokwi e Node-RED)
│   ├── relatorio_parte1.md   # Relatório 1: Borda, sensores e armazenamento offline
│   └── relatorio_parte2.md   # Relatório 2: MQTT, nuvem, Node-RED e testes
│
└── README.md                  # Este documento
```

---

## 3. Mapeamento de Hardware e Conexões (Wokwi)

| Componente | Pino do Componente | Pino do ESP32 | Função no Sistema |
| :--- | :--- | :--- | :--- |
| **DHT22** | VCC | 3V3 | Alimentação (3.3V) |
| **DHT22** | GND | GND | Aterramento |
| **DHT22** | SDA (Dados) | **GPIO 15** | Leitura de Temperatura Axilar e Umidade |
| **Potenciômetro** | VCC | 3V3 | Alimentação do divisor de tensão |
| **Potenciômetro** | GND | GND | Aterramento |
| **Potenciômetro** | SIG (Sinal) | **GPIO 34** (ADC1) | Simulação de Frequência Cardíaca (40 a 180 BPM) |
| **LED Verde** | Ânodo (+) via 220 $\Omega$ | **GPIO 2** | *Heartbeat* / Status Normal |
| **LED Vermelho** | Ânodo (+) via 220 $\Omega$ | **GPIO 4** | Alerta Clínico Crítico (Borda) |
| **Buzzer Piezo** | Pino Positivo | **GPIO 5** | Alarme Sonoro de Taquicardia / Febre |

> [!IMPORTANT]
> **Canal ADC1 (GPIO 34):** No ESP32, as portas do canal ADC2 (como GPIO 2, 4, 15) não podem ser utilizadas para leitura analógica enquanto o rádio Wi-Fi estiver ativo. Por isso, a leitura do potenciômetro foi alocada estritamente no pino **GPIO 34**, pertencente ao bloco ADC1, garantindo leituras analógicas contínuas e sem ruído de rádio.

---

## 4. Guia Rápido de Execução

### Opção 1: Teste Imediato no Wokwi (Sem necessidade de cadastro no HiveMQ)
O código já está pré-configurado para que você possa testar tudo em menos de 2 minutos:
1. Abra o [Wokwi Web](https://wokwi.com/projects/new/esp32).
2. Carregue os arquivos da pasta `firmware/` e o `wokwi/diagram.json`.
3. Certifique-se de que no `config.h` a diretiva `#define MQTT_USE_TLS` esteja como `false` (usando `broker.hivemq.com` na porta 1883).
4. Clique em **Iniciar Simulação**. O ESP32 conectará na rede `Wokwi-GUEST` e começará a transmitir os dados imediatamente!

### Opção 2: Conexão Corporativa ao HiveMQ Cloud (TLS 8883)
1. Crie seu cluster gratuito no [HiveMQ Cloud Console](https://console.hivemq.cloud/).
2. Crie um usuário e senha na aba **Access Management**.
3. Em `firmware/config.h`, defina:
   ```cpp
   #define MQTT_USE_TLS   true
   #define MQTT_BROKER    "SEU_CLUSTER.s1.eu.hivemq.cloud"
   #define MQTT_PORT      8883
   #define MQTT_USERNAME  "cardioia_user"
   #define MQTT_PASSWORD  "SuaSenhaAqui"
   ```
4. As instruções detalhadas estão no arquivo [docs/configuracao.md](file:///C:/Users/yasmi/.gemini/antigravity/scratch/CardioIA/docs/configuracao.md).

---

## 5. Como Executar o Dashboard no Node-RED

1. Instale o pacote do dashboard no terminal:
   ```bash
   npm install -g node-red
   cd ~/.node-red && npm install node-red-dashboard
   ```
2. Inicie o Node-RED digitando `node-red` no terminal.
3. Abra `http://localhost:1880/` no navegador.
4. Clique no Menu -> **Import** -> Selecione o arquivo [`node-red/flows.json`](file:///C:/Users/yasmi/.gemini/antigravity/scratch/CardioIA/node-red/flows.json).
5. No nó de configuração `HiveMQ Broker`, configure o endereço do broker escolhido (seja o cluster HiveMQ Cloud com TLS ou o `broker.hivemq.com` aberto).
6. Clique no botão vermelho **Deploy**.
7. Abra a interface visual no navegador:
   ```text
   http://localhost:1880/ui
   ```

---

## 6. Demonstração do Fluxo Offline e Sincronização (Roteiro para Apresentação)

Para demonstrar a resiliência offline exigida na avaliação da FIAP:

1. **Estado Normal (Online):**
   - Com a simulação rodando, observe no Monitor Serial as mensagens `[TELEMETRIA] Leitura transmitida em tempo real para o HiveMQ Cloud`.
   - Veja os valores atualizando no Dashboard do Node-RED a cada 3 segundos.

2. **Simulando Queda de Rede:**
   - No código `config.h`, altere temporariamente o nome do Wi-Fi para `"Wokwi-DESCONECTADO"` ou bloqueie o tráfego do broker.
   - O ESP32 imprimirá:
     ```text
     [WI-FI] >>> DESCONEXAO DETECTADA! Dispositivo operando em Modo Borda Resiliente (Offline) <<<
     [OFFLINE RESILIENCE] Registro #1 retido localmente. Fila: 1/60 pendentes.
     [OFFLINE RESILIENCE] Registro #2 retido localmente. Fila: 2/60 pendentes.
     ```
   - Altere a posição do potenciômetro durante esse período. As leituras continuam sendo registradas com seus timestamps locais!
   - No Node-RED, após 15 segundos o **Watchdog** identificará a ausência de sinal e mudará o status para `OFFLINE (TIMEOUT)` exibindo o alerta visual vermelho.

3. **Restaurando a Conexão e Sincronizando (Zero Data Loss):**
   - Restaure a rede para `"Wokwi-GUEST"`.
   - Assim que o ESP32 reconectar, o monitor serial exibirá:
     ```text
     [CONEXAO RESTAURADA] Reconectado a nuvem! X registros pendentes para sincronizar.
     [SINCRONIZACAO] Transmitindo registro pendente #1...
     [OFFLINE RESILIENCE] Confirmado envio do registro #1 com sucesso. Restam X-1 pendentes.
     ```
   - O Node-RED receberá a fila sequencial de dados com o status `ONLINE (DADOS RECUPERADOS)` e esvaziará a contagem até zero, sem qualquer perda de leitura histórica!
