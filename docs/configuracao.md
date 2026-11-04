# GUIA DE CONFIGURAÇÃO PASSO A PASSO
## Projeto CardioIA – FIAP

Este documento orienta detalhadamente como configurar cada componente do projeto **CardioIA**: o broker HiveMQ Cloud, a simulação no Wokwi, o dashboard no Node-RED e os utilitários de teste em Python.

---

## 1. Configuração do HiveMQ Cloud (Broker MQTT)

O **HiveMQ Cloud** oferece um plano gratuito permanente (*Serverless Free Tier*) de até 100 conexões com criptografia TLS/SSL, ideal para o protótipo.

### Passo a passo para criação do Cluster:
1. Acesse o portal: [https://console.hivemq.cloud/](https://console.hivemq.cloud/)
2. Crie uma conta gratuita ou faça login com sua conta Google/GitHub.
3. No painel de controle, clique em **"Create Cluster"**.
4. Selecione o plano **Free** (0 USD/mês).
5. Escolha a região de nuvem mais próxima (ex: Frankfurt, AWS ou Azure).
6. Aguarde cerca de 1 a 2 minutos até que o status do Cluster mude para **"RUNNING"**.
7. Na página inicial do seu cluster, localize o **Cluster URL**. Exemplo:
   ```text
   b49a1d5c7f8e4a908d123456789abcde.s1.eu.hivemq.cloud
   ```
   *Guarde esta URL! Ela será o endereço do seu broker MQTT.*

### Passo a passo para criar as Credenciais de Acesso:
1. No menu superior do seu cluster, clique na aba **"Access Management"**.
2. No campo **Username**, defina um usuário (exemplo: `cardioia_user`).
3. No campo **Password**, crie uma senha segura (exemplo: `CardioIA@2026`).
4. Clique em **"Add"**.
5. Assegure-se de que a permissão esteja como **"Publish & Subscribe"**.

---

## 2. Configuração do Firmware do ESP32 (`firmware/config.h`)

Abra o arquivo [`firmware/config.h`](file:///C:/Users/yasmi/.gemini/antigravity/scratch/CardioIA/firmware/config.h) e edite as seguintes diretivas com os dados obtidos no passo anterior:

```cpp
// 1. Defina se usara TLS ou broker publico
#define MQTT_USE_TLS        true  // Mantenha true para HiveMQ Cloud

// 2. Insira a URL do seu Cluster HiveMQ Cloud
#define MQTT_BROKER         "SEU_CLUSTER_HIVEMQ.s1.eu.hivemq.cloud"
#define MQTT_PORT           8883

// 3. Insira o usuario e senha criados no Access Management
#define MQTT_USERNAME       "cardioia_user"
#define MQTT_PASSWORD       "CardioIA@2026"
```

> [!TIP]
> **Modo de Teste Imediato Sem Cadastro:**
> Caso queira testar a simulação imediatamente sem criar conta na nuvem, basta alterar `#define MQTT_USE_TLS false` em `config.h`. O ESP32 conectará automaticamente ao `broker.hivemq.com` na porta `1883`, permitindo testar toda a lógica em segundos!

---

## 3. Execução da Simulação no Wokwi

### Opção A: Pelo Navegador (Wokwi Web)
1. Acesse: [https://wokwi.com/projects/new/esp32](https://wokwi.com/projects/new/esp32)
2. Substitua o conteúdo da aba **sketch.ino** pelo código de [`firmware/cardioia.ino`](file:///C:/Users/yasmi/.gemini/antigravity/scratch/CardioIA/firmware/cardioia.ino).
3. Crie novas abas clicando na seta ao lado dos arquivos e crie:
   - `config.h` (copie o conteúdo de `firmware/config.h`)
   - `sensors.h` (copie o conteúdo de `firmware/sensors.h`)
   - `offline_manager.h` (copie o conteúdo de `firmware/offline_manager.h`)
   - `mqtt_manager.h` (copie o conteúdo de `firmware/mqtt_manager.h`)
4. Clique na aba **diagram.json** e cole o conteúdo de [`wokwi/diagram.json`](file:///C:/Users/yasmi/.gemini/antigravity/scratch/CardioIA/wokwi/diagram.json).
5. Clique na aba **Library Manager** (ícone de livro) e instale ou cole em `libraries.txt`:
   ```text
   DHT sensor library
   Adafruit Unified Sensor
   PubSubClient
   ArduinoJson
   ```
6. Clique no botão verde de **Play (Iniciar Simulação)**.

### Opção B: Pelo VS Code com Extensão Wokwi
1. Instale a extensão oficial **Wokwi Simulator** no VS Code.
2. Abra a pasta `CardioIA/wokwi` no VS Code.
3. Abra o arquivo `diagram.json` e pressione `F1` -> `Wokwi: Start Simulator`.

---

## 4. Configuração do Dashboard no Node-RED

### 4.1 Pré-requisitos
Certifique-se de possuir o Node-RED instalado em seu computador:
```bash
npm install -g --unsafe-perm node-red
```
Instale a biblioteca do dashboard no diretório do Node-RED (geralmente `~/.node-red`):
```bash
cd ~/.node-red
npm install node-red-dashboard
```

### 4.2 Iniciar o Node-RED
No terminal, execute:
```bash
node-red
```
O Node-RED inicializará e indicará o endereço: `http://localhost:1880/`.

### 4.3 Importar o Fluxo do CardioIA
1. Acesse `http://localhost:1880/` no seu navegador.
2. No canto superior direito, clique no menu hambúrguer (ícone de três linhas) e vá em **Import**.
3. Clique em **select a file to import** e selecione o arquivo [`node-red/flows.json`](file:///C:/Users/yasmi/.gemini/antigravity/scratch/CardioIA/node-red/flows.json) (ou abra o arquivo, copie todo o JSON e cole no campo de texto).
4. Clique em **Import**.

### 4.4 Ajustar o Nó do Broker MQTT no Node-RED
1. Dê um duplo clique no nó **Sub Telemetria Consolidada**.
2. Ao lado de **Server**, clique no ícone de lápis para editar o nó de configuração `HiveMQ Broker`.
3. Ajuste os parâmetros de conexão:
   - Se estiver usando o **HiveMQ Cloud (Porta 8883)**:
     - **Server:** `seu-cluster.s1.eu.hivemq.cloud`
     - **Port:** `8883`
     - Marque a caixa **Use TLS**.
     - Na aba **Security**, preencha o **Username** e **Password**.
   - Se estiver usando o **Broker Público de Teste (Porta 1883)**:
     - **Server:** `broker.hivemq.com`
     - **Port:** `1883`
     - Desmarque a caixa **Use TLS**.
     - Na aba **Security**, deixe em branco.
4. Clique em **Update** e em seguida em **Done**.
5. Clique no botão vermelho **Deploy** no canto superior direito para aplicar as alterações.

### 4.5 Visualizar o Dashboard Clínico
Abra no navegador o endereço do Dashboard:
```text
http://localhost:1880/ui
```
Você verá a interface com prontuário do paciente, gauges de batimento cardíaco, temperatura axilar, umidade, gráfico de histórico temporal e banner de alertas em tempo real.

---

## 5. Execução dos Utilitários em Python

Na pasta `CardioIA/python`:

1. Instale a biblioteca do cliente MQTT:
   ```bash
   pip install -r requirements.txt
   ```
2. Para monitorar as mensagens que chegam no broker via terminal:
   ```bash
   python cardioia_subscriber.py
   ```
3. Para simular o vestível e testar o dashboard sem abrir o Wokwi:
   ```bash
   python mock_publisher.py
   ```
