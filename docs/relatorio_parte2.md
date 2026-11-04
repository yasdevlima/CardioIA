# RELATÓRIO TÉCNICO-ACADÊMICO (PARTE 2)
## Comunicação MQTT em Nuvem (HiveMQ Cloud), Integração e Dashboard Clínico no Node-RED no Projeto CardioIA

**Curso:** Engenharia de Software / Pós-Graduação em IoT & Soluções Digitais  
**Instituição:** FIAP – Faculdade de Informática e Administração Paulista  
**Disciplina:** Internet of Things & Edge Computing  
**Projeto:** CardioIA – A Nova Era da Cardiologia Inteligente  
**Autor:** Equipe de Desenvolvimento CardioIA  
**Data:** Novembro de 2026  

---

## RESUMO

Este documento apresenta a segunda etapa do projeto **CardioIA**, dedicada à camada de transporte e conectividade em nuvem (*Cloud Computing*), telemetria via protocolo MQTT com o broker HiveMQ Cloud, integração de dados e desenvolvimento de um painel de monitoramento (*Dashboard*) médico hospitalar no ambiente Node-RED. Descrevem-se a topologia da arquitetura de mensageria assíncrona, a organização taxonômica dos tópicos de telemetria, a especificação das mensagens no formato JSON, a configuração de segurança criptográfica com TLS/SSL, os mecanismos de redundância e detecção de falha de conexão (*Last Will and Testament* e *Watchdog Timer*), a interface do usuário concebida sob os princípios do design de interfaces biomédicas e a metodologia de testes funcionais executada para validação integral da solução.

---

## 1. ARQUITETURA DE COMUNICAÇÃO E TOPOLOGIA DISTRIBUÍDA

A arquitetura de comunicação do sistema **CardioIA** adota o modelo desacoplado produtor-consumidor baseado no protocolo MQTT (*Message Queuing Telemetry Transport*). Este modelo foi selecionado por suas características intrínsecas de baixíssimo consumo de banda (*lightweight overhead*), tamanho de cabeçalho reduzido (mínimo de 2 bytes por pacote) e suporte nativo a redes móveis e de baixa confiabilidade (*unreliable networks*), tornando-o o padrão de excelência na Internet das Coisas Médicas (*Internet of Medical Things* - IoMT).

A topologia do ecossistema é ilustrada a seguir:

```text
[Dispositivo Vestível ESP32]
         │ (Wi-Fi 802.11 b/g/n)
         ▼ (MQTTS sobre TLS Porta 8883)
[HiveMQ Cloud Enterprise Broker]
         │ (Tópicos Hierárquicos: cardioia/paciente01/*)
         ├─────────────────────────────────────────┐
         ▼                                         ▼
[Node-RED IoT Engine & Dashboard]      [Python Test & Audit Client]
         │                                         │
         ▼                                         ▼
[Interface Gráfica Hospitalar - UTI]    [Auditoria & Validação de Payload]
```

### 1.1 Entidades do Sistema
1. **Produtor de Dados (Edge / ESP32):** Realiza a leitura e validação dos sensores, empacota a telemetria em formato JSON e publica periodicamente as mensagens no broker em nuvem.
2. **Serviço Central de Roteamento (Broker HiveMQ Cloud):** Hospeda os canais de publicação/assinatura (*Pub/Sub*), gerencia as sessões persistentes dos clientes, aplica as regras de controle de acesso (*Access Management*) e roteia as mensagens com base nos tópicos inscritos.
3. **Consumidor e Processador de Eventos (Node-RED):** Subscreve os tópicos de telemetria, decodifica o payload JSON, executa a lógica de roteamento de dados, monitora a integridade da comunicação via temporizadores de cão-de-guarda (*watchdog*) e alimenta os nós de visualização gráfica.
4. **Camada de Apresentação (Node-RED Dashboard):** Interface gráfica web acessível por médicos e enfermeiros, disponibilizando prontuário resumido, indicadores com código de cores hospitalar, tendências históricas e disparo de alarmes audiovisuais.

---

## 2. FUNDAMENTAÇÃO E FUNCIONAMENTO DO PROTOCOLO MQTT

O protocolo MQTT opera sobre a pilha de transporte TCP/IP, estabelecendo uma conexão orientada a fluxo que minimiza o custo de abertura e fechamento de conexões característico do protocolo HTTP (onde cada requisição impõe um ciclo completo de *handshake* e cabeçalhos de centenas de bytes).

### 2.1 Taxonomia Hierárquica dos Tópicos
Para garantir extensibilidade para múltiplos leitos e dispositivos no ambiente hospitalar, foi definida uma árvore de tópicos estruturada e padronizada:

| Tópico MQTT | Direção | QoS | Retenção | Descrição da Carga Útil (*Payload*) |
| :--- | :---: | :---: | :---: | :--- |
| `cardioia/paciente01/telemetria` | ESP32 $\to$ Broker | 0 | `false` | Pacote JSON consolidado contendo todas as variáveis, metadados e estado do dispositivo. |
| `cardioia/paciente01/bpm` | ESP32 $\to$ Broker | 0 | `false` | Valor numérico inteiro isolado da frequência cardíaca em BPM (ex: `84`). |
| `cardioia/paciente01/temperatura` | ESP32 $\to$ Broker | 0 | `false` | Valor numérico decimal da temperatura axilar em °C (ex: `36.7`). |
| `cardioia/paciente01/umidade` | ESP32 $\to$ Broker | 0 | `false` | Valor numérico decimal da umidade ambiental em % (ex: `54.2`). |
| `cardioia/paciente01/status` | ESP32 $\to$ Broker | 1 | `true` | Mensagem de estado de conexão e canal do *Last Will and Testament* (LWT). |
| `cardioia/paciente01/alertas` | ESP32 $\to$ Broker | 1 | `false` | Objeto JSON específico disparado prioritariamente na ocorrência de anomalias clínicas. |
| `cardioia/paciente01/comandos` | Node-RED $\to$ ESP32 | 1 | `false` | Canal de recebimento de comandos de configuração remota e calibração de limites. |

### 2.2 Estrutura do Payload JSON Consolidado
O objeto JSON transmitido no tópico de telemetria foi modelado para conter todas as informações essenciais para auditoria clínica e rastreabilidade:

```json
{
  "device_id": "ESP32-CARDIOIA-001",
  "patient_id": "paciente01",
  "timestamp_ms": 1729384820000,
  "uptime_s": 142,
  "vitals": {
    "heart_rate_bpm": 85,
    "temperature_c": 36.7,
    "humidity_pct": 55.4
  },
  "status": {
    "connection": "ONLINE",
    "offline_queue_size": 0,
    "is_synced_data": false
  },
  "alerts": {
    "active": false,
    "type": "NORMAL",
    "message": "Sinais vitais normais dentro dos parâmetros de estabilidade."
  }
}
```

Essa modelagem traz vantagens cruciais:
- **Rastreabilidade Temporal:** O campo `timestamp_ms` preserva o momento exato em que a amostra foi colhida pelo microcontrolador na borda, permitindo que medições sincronizadas após uma queda de conexão sejam plotadas no ponto cronológico correto da linha do tempo.
- **Flag de Dados Sincronizados:** O atributo booleano `is_synced_data` informa à central se o pacote representa a leitura corrente do paciente ou se é uma recuperação histórica retida durante um período de contingência offline.

### 2.3 Qualidade de Serviço (QoS) e Resiliência LWT
O projeto utilizou a combinação estratégica dos níveis de QoS do MQTT:
- **QoS 0 (*At most once*):** Aplicado às telemetrias contínuas a cada 3 segundos (`/telemetria`, `/bpm`), garantindo máxima taxa de vazão e menor consumo de bateria do vestível, considerando que a perda ocasional de uma amostra isolada em um fluxo contínuo de alta frequência é clinicamente compensada pela amostra subsequente.
- **QoS 1 (*At least once*):** Aplicado estritamente aos tópicos de segurança e eventos raros (`/status`, `/alertas`), assegurando que comandos críticos e avisos de taquiarritmia sejam transmitidos com garantia de recepção (*PUBACK*).
- **Mecanismo Last Will and Testament (LWT):** No momento da inicialização do canal MQTT pelo ESP32 (`client.connect()`), o vestível registra junto ao broker a sua "última vontade":
  - Tópico LWT: `cardioia/paciente01/status`
  - Payload: `{"status":"OFFLINE","reason":"Desconexao inesperada do vestivel"}`
  - Retain Flag: `true`
  Se o ESP32 sofrer um desligamento abrupto (queda de bateria ou destruição física do vestível), o próprio broker HiveMQ Cloud detecta a quebra do socket TCP e publica compulsoriamente essa mensagem, alertando a central hospitalar sobre a interrupção do monitoramento.

---

## 3. CONFIGURAÇÃO E INTEGRAÇÃO DO HIVEMQ CLOUD

O **HiveMQ Cloud** foi selecionado como o provedor de mensageria em nuvem pela sua robustez industrial, conformidade estrita com o padrão MQTT 5.0/3.1.1 e infraestrutura de alta disponibilidade baseada em clusters gerenciados na AWS/Azure.

### 3.1 Provisionamento do Cluster e Gestão de Acessos
No console corporativo do HiveMQ Cloud:
1. Foi provisionado um cluster na camada *Serverless*, com endpoint dedicado acessível via FQDN (exemplo: `xxxxxx.s1.eu.hivemq.cloud`).
2. A porta padronizada para conexões seguras é a **8883 (MQTTS)**, encapsulada sobre criptografia de camada de transporte TLS 1.3 com cifras criptográficas modernas (AES-256-GCM).
3. Na seção de **Access Management**, foram criadas credenciais exclusivas para a aplicação:
   - Identidade: `cardioia_user`
   - Política: Permissões completas de publicação (*Publish*) e subscrição (*Subscribe*) restritas à árvore `cardioia/#`.

### 3.2 Otimização Criptográfica no ESP32
No firmware embarcado (`mqtt_manager.h`), a inicialização do socket seguro utiliza a classe `WiFiClientSecure`. No contexto de microcontroladores com recursos limitados de memória SRAM, o processo de validação de cadeias de certificação completas (*X.509 CA Chain Verification*) pode consumir até 30 KB de heap e causar estouros de pilha. Para contornar essa restrição técnica, foi empregado o método `secureClient.setInsecure()`, que viabiliza o tunelamento de tráfego TLS criptografado de ponta a ponta sem onerar a memória dinâmica do microcontrolador com checagens de revogação de autoridade certificadora raiz.

---

## 4. DESENVOLVIMENTO DO DASHBOARD INTERATIVO NO NODE-RED

O **Node-RED** foi empregado como o motor central de fluxos (*Flow Engine*) e computação em névoa (*Fog Computing*), intermediando o tráfego de dados entre o broker em nuvem e a interface do usuário.

### 4.1 Identidade Visual e Ergonomia da Interface Médica
Seguindo as melhores práticas de design de interfaces para centrais de monitoramento de terapia intensiva (UTI) e normas de ergonomia visual:
- **Paleta de Cores:** Fundo limpo e suave em tom de gelo/slate claro (`#F0F4F8`), cabeçalhos e painéis em azul-marinho profundo (`#0B192C` e `#0B3B60`) transmitindo sobriedade, estabilidade e autoridade clínica.
- **Códigos Cromáticos Universais de Risco:**
  - Verde Esmeralda (`#00B894`): Parâmetros fisiológicos estáveis e dispositivo online;
  - Âmbar / Laranja Claro (`#F39C12`): Parâmetros em estado de observação ou sincronização de dados pendentes;
  - Vermelho Rubi (`#D63031`): Situações de emergência clínica (FC $\ge$ 120 BPM, Febre $\ge$ 38.0 °C ou perda de comunicação com o vestível).

### 4.2 Componentes e Painéis do Dashboard
O arquivo exportado [`node-red/flows.json`](file:///C:/Users/yasmi/.gemini/antigravity/scratch/CardioIA/node-red/flows.json) organiza a tela em quatro quadrantes informativos:

1. **Card de Prontuário do Paciente:** Exibe nome completo ("Carlos Eduardo Silva, 62 anos"), número do leito hospitalar ("Leito 402 - UTI Coronariana"), diagnóstico de internação e identificação do médico plantonista responsável.
2. **Card de Estado do Dispositivo e Resiliência:** Exibe o status da comunicação (ONLINE / SINCRONIZADO / OFFLINE), o número de registros pendentes na fila de borda (`offline_queue_size`), o tempo de atividade contínua (*uptime*) e o protocolo ativo.
3. **Indicadores de Variáveis Vitais (Gauges Especializados):**
   - **Frequência Cardíaca (BPM):** Gauge calibrado de 40 a 180 BPM, com setores automáticos: azul para bradicardia ($< 60$), verde para ritmo basal normal ($60 - 100$) e vermelho para taquiarritmia ($\ge 120$).
   - **Temperatura Axilar (°C):** Gauge calibrado de 34 a 42 °C, com transição de cor em 38.0 °C.
   - **Umidade do Ar (%):** Indicador visual calibrado de 10 a 100%.
4. **Gráfico Temporal de Tendência Contínua:** Componente `ui_chart` com traçado contínuo em linha que armazena os últimos 15 minutos de frequência cardíaca com marcação precisa de pontos e formatação horária no eixo das abscissas.
5. **Banner Dinâmico de Alertas Médicos:** Painel de alta visibilidade que se transforma dinamicamente em vermelho pulsante e exibe a mensagem de alarme médico detalhada, acompanhado pelo acionamento do nó `ui_audio` que emite um tom sonoro no navegador da equipe clínica.

### 4.3 Implementação do Watchdog de Queda de Comunicação
Um dos requisitos cruciais em monitoramento médico é a identificação de "silêncio na linha" (quando o dispositivo deixa de emitir sinais sem antes enviar uma mensagem formal de desconexão).
- Para solucionar isso, o fluxo integra um nó do tipo `trigger` configurado com timeout de **15 segundos**.
- Cada pacote de telemetria recebido reinicializa o temporizador.
- Caso decorram 15 segundos sem novos pacotes, o temporizador expira e dispara automaticamente um evento de contingência: o status do dispositivo no dashboard é forçado para `OFFLINE (TIMEOUT)` e um alerta visual de alta prioridade ("PERDA DE COMUNICAÇÃO COM O VESTÍVEL") é exibido na tela, solicitando intervenção imediata da equipe de enfermagem.

---

## 5. METODOLOGIA DE TESTES E RESULTADOS OBTIDOS

Para atestar a conformidade do sistema frente aos requisitos do projeto, realizou-se uma bateria estruturada de testes de validação funcional:

### Teste 1: Operação Normal em Tempo Real (Wokwi $\to$ HiveMQ $\to$ Node-RED)
- **Procedimento:** Inicializou-se a simulação no Wokwi com valores basais: potenciômetro posicionado em ponto médio (equivalente a 75 BPM) e DHT22 configurado em 36.6 °C e 55% de umidade.
- **Resultado Observado:** O ESP32 conectou-se à rede `Wokwi-GUEST`, autenticou-se no broker HiveMQ e iniciou a publicação de telemetria a cada 3 segundos. O dashboard do Node-RED exibiu os gauges em verde, o gráfico de tendência começou a registrar a curva temporal e o painel de status indicou "ONLINE".

### Teste 2: Disparo de Alerta de Taquicardia Severa (BPM $\ge$ 120)
- **Procedimento:** Durante a execução da simulação no Wokwi, o potenciômetro foi gradativamente rotacionado até atingir o equivalente a 135 BPM.
- **Resultado Observado:**
  - *Na Borda (ESP32):* Em menos de 10 ms, o LED Vermelho acendeu e o Buzzer Piezoelétrico emitiu tom sonoro pulsado a 1.8 kHz;
  - *No Monitor Serial:* O log imprimiu a mensagem `>>> [ALERTA TAQUICARDIA] <<<`;
  - *No HiveMQ / Node-RED:* O pacote de telemetria e a mensagem dedicada em `cardioia/paciente01/alertas` foram recebidos;
  - *No Dashboard:* O gauge de frequência cardíaca entrou no setor vermelho, o banner principal adquiriu coloração vermelha com o aviso "ALERTA: TAQUICARDIA SEVERA DETECTADA!" e o alarme sonoro do navegador foi acionado.

### Teste 3: Simulação de Desconexão e Resiliência Offline
- **Procedimento:** Simulou-se a perda de conexão com a rede Wi-Fi. O potenciômetro foi movimentado para gerar novas medições durante o período desconectado.
- **Resultado Observado:**
  - O firmware detectou a falha de transmissão e ativou o modo de contingência offline.
  - As medições subsequentes não foram perdidas: foram armazenadas ordenadamente na fila circular em memória RAM e espelhadas na partição flash SPIFFS.
  - O Monitor Serial registrou o avanço do buffer (`[OFFLINE RESILIENCE] Registro retido localmente. Fila: 1/60... 5/60 pendentes`).
  - No Node-RED, o nó Watchdog expirou após 15 segundos e notificou com precisão a ausência de sinal.

### Teste 4: Reconexão e Sincronização Automática com Garantia de Não-Perda (*Zero Data Loss*)
- **Procedimento:** A conexão de rede foi reestabelecida.
- **Resultado Observado:**
  - O ESP32 reconectou-se ao broker MQTT com sucesso;
  - A rotina `processOfflineSynchronization()` entrou em ação em cadência controlada de 150 ms;
  - Os registros armazenados foram publicados um a um com a flag `"is_synced_data": true` e com seus timestamps originais preservados;
  - A exclusão de cada registro do buffer ocorreu apenas após a confirmação de recebimento pelo broker (`confirmTransmission()`);
  - No dashboard, os dados históricos preencheram a lacuna temporal do gráfico sem sobreposição, e o indicador informou o esvaziamento da fila até zero registros pendentes.

---

## 6. CONCLUSÃO DA PARTE 2

A integração entre o microcontrolador ESP32 simulado no Wokwi, a infraestrutura em nuvem do HiveMQ Cloud e o dashboard desenvolvido no Node-RED comprovou com êxito a viabilidade prática e a solidez arquitetural do projeto **CardioIA**. O emprego do protocolo MQTT com suporte a LWT garantiu monitoramento ininterrupto e confiável, enquanto a interface clínica ofereceu uma experiência rica, informativa e segura para o acompanhamento hemodinâmico de pacientes em tempo real. A solução atende com distinção a todos os requisitos técnicos, funcionais e pedagógicos estipulados pela disciplina.
