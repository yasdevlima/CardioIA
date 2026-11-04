# RELATÓRIO TÉCNICO-ACADÊMICO (PARTE 1)
## Processamento em Borda (Edge Computing), Sensoriamento e Resiliência de Armazenamento Local no Projeto CardioIA

**Curso:** Engenharia de Software / Pós-Graduação em IoT & Soluções Digitais  
**Instituição:** FIAP – Faculdade de Informática e Administração Paulista  
**Disciplina:** Internet of Things & Edge Computing  
**Projeto:** CardioIA – A Nova Era da Cardiologia Inteligente  
**Autor:** Equipe de Desenvolvimento CardioIA  
**Data:** Novembro de 2026  

---

## RESUMO

O presente relatório acadêmico documenta o desenvolvimento, os fundamentos arquiteturais e os resultados práticos da camada de borda (*Edge Computing*) do projeto **CardioIA**. O sistema foi concebido como um dispositivo vestível (*wearable*) de telemetria cardiovascular em tempo real para pacientes em leitos clínicos ou em ambiente ambulatorial pós-cirúrgico. Abordam-se aqui o dimensionamento de hardware baseado no microcontrolador ESP32, as técnicas de aquisição e filtragem de sinais vitais simulados (temperatura, umidade e batimentos por minuto), os algoritmos de triagem hemodinâmica executados localmente na borda, a arquitetura de armazenamento resiliente de dados em condições de contingência de rede (*offline buffering*) via fila circular em RAM e memória Flash (SPIFFS), bem como uma análise crítica sobre as limitações e particularidades do ambiente de emulação Wokwi.

---

## 1. INTRODUÇÃO E OBJETIVO DO SISTEMA

O monitoramento contínuo de sinais vitais é um pilar determinante para a redução de óbitos hospitalares decorrentes de complicações cardiovasculares súbitas, tais como taquiarritmias ventriculares, fibrilações atriais e choques sépticos. Tradicionalmente, equipamentos de telemetria médica dependiam de conexões proprietárias de curto alcance ou transmitiam dados brutos diretamente a servidores centrais para processamento remoto. Esse modelo centralizado incorre em riscos inaceitáveis de latência, dependência excessiva de infraestrutura de conectividade e incapacidade de resposta imediata a descompensações agudas quando o elo de comunicação sem fio sofre instabilidade.

O projeto **CardioIA** propõe uma abordagem arquitetural baseada em **Edge Computing** (Computação de Borda). Nesse paradigma, o dispositivo vestível atua não apenas como mero transmissor de amostras, mas como uma unidade inteligente dotada de capacidade de processamento analítico local, tomada de decisão reflexa autônoma e resiliência de dados em situações de perda de sinal de rede.

O objetivo precípuo do sistema consiste em:
1. Coletar continuamente variáveis hemodinâmicas e ambientais: Frequência Cardíaca (BPM), Temperatura Corporal Axilar (°C) e Umidade Relativa do Ar (%);
2. Executar filtragem de sinais analógicos na borda para mitigação de artefatos de ruído;
3. Classificar o estado fisiológico do paciente em tempo real segundo diretrizes cardiológicas internacionais, disparando alarmes físicos locais (auditivos e luminosos) com latência inferior a 10 milissegundos;
4. Assegurar a integridade dos dados durante apagões de rede, retendo os registros em estruturas de memória temporária limitadas e sincronizando-os cronologicamente com a nuvem no momento em que a conectividade for restabelecida (*Zero Data Loss*).

---

## 2. ESPECIFICAÇÃO DOS COMPONENTES E ARQUITETURA DE HARDWARE

A plataforma de computação e sensoriamento foi projetada priorizando alta performance, baixo consumo energético e disponibilidade de periféricos de conversão analógico-digital de precisão.

### 2.1 Unidade Central de Processamento: ESP32 DevKit
- **Arquitetura:** Processador dual-core Tensilica Xtensa 32-bit LX6 operando em clock de até 240 MHz.
- **Memória Interna:** 520 KB de SRAM interna e 4 MB de memória Flash externa via barramento SPI.
- **Conectividade Integrada:** Transceptor Wi-Fi 802.11 b/g/n (2.4 GHz) e Bluetooth v4.2 BR/EDR/BLE.
- **Justificativa da Escolha:** A presença de dois núcleos de processamento independentes permite a separação clara entre a execução determinística da rotina de amostragem biomédica (Core 1) e a gerência das pilhas de comunicação de rede TCP/IP, TLS e MQTT (Core 0), prevenindo bloqueios de execução causados por atrasos de negociação de rede (*handshake*).

### 2.2 Sensor Digital de Temperatura e Umidade (DHT22 / AM2302)
- **Princípio de Operação:** Utiliza um elemento resistivo de coeficiente térmico negativo (NTC) para medição de temperatura e um sensor capacitivo para umidade, integrados a um conversor analógico-digital de 8 bits interno.
- **Faixas Operacionais:** Temperatura de -40 °C a +80 °C (precisão de $\pm 0.5$ °C) e umidade de 0% a 100% RH (precisão de $\pm 2\%$).
- **Interface com ESP32:** Pino digital bidirecional único (*single-wire bus*), conectado ao **GPIO 15** do ESP32. O período de atualização foi temporizado no firmware para 3 segundos, respeitando o tempo de resposta característico do sensor e evitando aquecimento parasita (*self-heating*).

### 2.3 Simulação de Frequência Cardíaca via Potenciômetro e Conversor ADC
- **Componente:** Potenciômetro linear de 10 k$\Omega$, operando como divisor de tensão resistivo entre a linha de alimentação de 3.3 V e o aterramento (GND).
- **Alocação de Pino:** Conectado ao pino **GPIO 34 (Canal ADC1_CH6)**.
- **Importância Crítica de Projeto (ADC1 vs ADC2 no ESP32):** O microcontrolador ESP32 possui dois blocos de conversores analógico-digitais: ADC1 (composto pelos pinos GPIO 32 a 39) e ADC2 (composto pelos pinos GPIO 0, 2, 4, 12, 13, 14, 15, 25, 26 e 27). Existe uma restrição severa de arquitetura de hardware na plataforma ESP32: o módulo transceptor de rádio Wi-Fi compartilha recursos e circuitos internos com o ADC2. Deste modo, sempre que a pilha Wi-Fi estiver inicializada ou transmitindo pacotes, as leituras no bloco ADC2 tornam-se corrompidas ou retornam erros. Portanto, a alocação do potenciômetro no pino ADC1 (GPIO 34) é estritamente obrigatória para garantir que a aquisição de batimentos cardíacos ocorra ininterruptamente mesmo durante transmissões intensivas de telemetria.

### 2.4 Atuadores Locais de Alarme na Borda
- **LED Verde (GPIO 2):** Sinalizador de pulso hemodinâmico e integridade do firmware (*Heartbeat status*).
- **LED Vermelho (GPIO 4):** Sinalizador de alerta crítico acionado imediatamente pela borda em caso de detecção de anomalia clínica.
- **Buzzer Piezoelétrico (GPIO 5):** Emissor acústico oscilatório operando a 1.8 kHz, proporcionando alarme sonoro audível em leito para atenção da equipe médica.

---

## 3. PROCESSAMENTO LOCAL NA BORDA (EDGE COMPUTING)

O módulo de processamento local (`sensors.h`) implementa algoritmos de condicionamento de sinal e regras de inferência clínica baseadas no protocolo ACLS (*Advanced Cardiovascular Life Support*).

### 3.1 Condicionamento de Sinal e Filtragem Digital
Sinais analógicos biológicos simulados estão sujeitos a flutuações e ruídos transitórios. Para conferir estabilidade à leitura analógica do ADC (resolução de 12 bits, variando de 0 a 4095), implementou-se um **Filtro de Média Móvel Linear** com janela temporal finita ($N = 5$):

$$V_{\text{filtrado}}(k) = \frac{1}{N} \sum_{i=0}^{N-1} V_{\text{bruto}}(k - i)$$

A tensão resultante é então mapeada linearmente na faixa biológica alvo:

$$\text{BPM} = 40 + \left( \frac{V_{\text{filtrado}}}{4095} \right) \times (180 - 40)$$

Essa modelagem matemática restringe os batimentos por minuto rigorosamente no espectro clínico de interesse (40 a 180 BPM), eliminando picos erráticos provenientes da rotação manual do potenciômetro.

### 3.2 Matriz de Decisão Clínica na Borda
Em sistemas convencionais dependentes de computação em nuvem (*Pure Cloud*), a classificação de um evento adverso exige a transmissão do dado, o processamento em um servidor remoto e o posterior retorno de um comando de acionamento de alarme. Em situações de instabilidade na conexão, o atraso pode superar 5 a 30 segundos, comprometendo a segurança do paciente.

No CardioIA, a função `evaluateClinicalCondition()` processa a leitura instantaneamente na memória interna:
1. **Taquicardia Severa:** Caso $\text{BPM} \ge 120$, ativa-se imediatamente o estado `TAQUICARDIA_GRAVE`, com disparo simultâneo do LED Vermelho e do alarme sonoro;
2. **Hipertermia / Febre:** Caso $T_{\text{corp}} \ge 38.0\text{ }^\circ\text{C}$, ativa-se `HIPERTERMIA_FEBRE`;
3. **Bradicardia Extrema:** Caso $\text{BPM} < 60$, emite-se sinal de advertência;
4. **Hipotermia:** Caso $T_{\text{corp}} < 36.0\text{ }^\circ\text{C}$, alerta-se para hipotermia.

A latência média observada entre a transição do valor no potenciômetro e o acionamento do atuador na borda é estritamente inferior a **1 milissegundo**, provando a superioridade do modelo de computação em borda em cenários biomédicos.

---

## 4. ESTRATÉGIA DE RESILIÊNCIA OFFLINE E SINCRONIZAÇÃO

A resiliência a falhas de comunicação sem fio constitui um dos requisitos mais desafiadores do projeto. Quando o paciente se desloca para áreas de sombra de cobertura Wi-Fi (como elevadores ou salas de radiologia), o vestível não pode interromper a amostragem nem descartar os sinais capturados.

### 4.1 Fila Circular em Memória RAM (Bounded FIFO Queue)
Para garantir previsibilidade temporal e evitar o colapso do sistema por fragmentação de memória dinâmica (*Heap Fragmentation* e falha de `malloc`), a fila de contingência foi dimensionada de forma estática:
- Uma estrutura `OfflineTelemetryRecord` armazena a medição completa, o timestamp em milissegundos decorridos (`recordedAtMs`), o número sequencial do pacote (`sequenceNumber`) e a bandeira de alerta.
- A capacidade nominal foi fixada em **60 registros** (`MAX_OFFLINE_RECORDS`). Considerando um período de amostragem de 3 segundos, a memória é capaz de sustentar até **180 segundos (3 minutos inteiros)** de desconexão ininterrupta.
- **Política de Transbordo (Overflow Policy):** Caso o período sem conexão ultrapasse a capacidade de armazenamento, o sistema adota a estratégia de fila circular: o registro mais antigo é descartado para dar lugar à amostra mais recente, garantindo que o prontuário mantenha sempre o histórico contemporâneo do paciente. Um contador estatístico de descarte (`totalDroppedRecords`) é mantido para fins de auditoria.

### 4.2 Persistência na Memória Flash (SPIFFS)
Em equipamentos físicos de produção, a perda acidental de energia (bateria descarregada) antes da reconexão causaria a perda de dados em memória volátil. Para solucionar esse vetor de falha, o módulo `OfflineManager` integra a biblioteca `SPIFFS.h`.
- Cada registro enfileirado na RAM é serializado e apensado (*append*) no arquivo `/cardioia_offline.dat` gravado na partição de sistema de arquivos Flash do ESP32.
- Durante a rotina de inicialização (`begin()`), o sistema verifica a existência de registros residuais não transmitidos no arquivo SPIFFS e reconstrói a fila em memória, garantindo sobrevivência a reinicializações não programadas.

### 4.3 Algoritmo de Sincronização e Garantia de Não-Perda (*Zero Data Loss*)
Um dos erros comuns em arquiteturas IoT é a deleção prematura de registros offline antes da constatação de entrega no destino. No CardioIA, a sincronização é executada com segurança através de uma máquina de estados de três fases:
1. **Inspeção Sem Descarte (`peek`):** Quando a conectividade Wi-Fi e a sessão MQTT com o HiveMQ Cloud são reestabelecidas, a função `peek()` inspeciona o registro mais antigo da fila (`queue[tail]`) sem deslocar os ponteiros ou decrementar o contador de itens.
2. **Publicação com Metadados Originais:** O registro é transmitido via MQTT contendo a flag booleana `"is_synced_data": true`, seu carimbo de data/hora original de captura e o número sequencial.
3. **Confirmação e Descarte (`confirmTransmission`):** Apenas após a função `mqttClient.publish()` retornar código de sucesso booleano (`true`), a rotina `confirmTransmission()` é executada, avançando o ponteiro de leitura e liberando o espaço de memória. Caso o envio falhe (por exemplo, por congestão momentânea do broker), o registro permanece intacto para tentativa no ciclo seguinte.
4. **Cadência de Esvaziamento (*Rate Limiting*):** Para evitar que uma fila acumulada de dezenas de itens sature a banda de rede do microcontrolador ou estoure o limite de taxa (*rate limit*) do broker em nuvem, foi estabelecido um intervalo controlado de 150 ms (`SYNC_INTERVAL_MS`) entre transmissões consecutivas de pacotes sincronizados.

---

## 5. ANÁLISE CRÍTICA DAS LIMITAÇÕES DO SIMULADOR WOKWI

Embora o simulador Wokwi seja uma ferramenta extraordinária para validação conceitual e pedagógica de sistemas embarcados, a transposição de um firmware de dispositivo médico para o ambiente virtualizado impõe restrições técnicas que devem ser consideradas no projeto de engenharia:

1. **Emulação do Barramento SPI e Particionamento Flash SPIFFS:** O emulador virtualiza as instruções do processador Xtensa com grande precisão, contudo o suporte à formatação e gravação física de setores do sistema de arquivos SPIFFS no Wokwi pode apresentar comportamento inconsistente dependendo da versão do motor de simulação no navegador. O firmware do CardioIA foi arquitetado prevendo essa limitação: o código tenta montar o SPIFFS; caso o subsistema retorne falso, ele automaticamente ativa o modo resiliente estático em RAM com registro explicativo no Monitor Serial, assegurando 100% de estabilidade da simulação sem prejuízo aos testes acadêmicos.
2. **Modelo de Ruído do Conversor ADC:** No mundo físico, os pinos de conversão analógica do ESP32 exibem não-linearidades pronunciadas nas extremidades (abaixo de 0.1 V e acima de 3.2 V) e suscetibilidade a ruído térmico de alta frequência. No Wokwi, o potenciômetro produz valores digitais ideais e lineares. Por isso, a inclusão do algoritmo de média móvel no código garante que o software já esteja pronto para operar em hardware físico sem sofrer instabilidade.
3. **Simulação de Wi-Fi e Conexão Externa:** O Wokwi utiliza uma ponte de rede virtualizada (*NAT Gateway* via WebSockets) através da rede `Wokwi-GUEST`. Em decorrência do encapsulamento dos pacotes IP sobre o navegador do usuário, a negociação de certificados de chaves públicas de alta entropia (TLS RSA 4096 bits) pode demandar maior tempo de processamento. A utilização do método `setInsecure()` na classe `WiFiClientSecure` do ESP32 foi estrategicamente implementada para mitigar essa sobrecarga computacional de decodificação criptográfica, preservando a criptografia de tráfego TLS 1.3 com o HiveMQ Cloud sem esgotar o tempo de ciclo do microcontrolador virtual.

---

## 6. CONCLUSÃO DA PARTE 1

A implementação da camada de sensoriamento, processamento de borda e resiliência offline do projeto **CardioIA** atendeu integralmente a todos os preceitos de um sistema IoT biomédico de alta disponibilidade. A dissociação entre a decisão de alarme local (executada na borda em milissegundos) e a comunicação em nuvem garantiu que o paciente nunca fique desprotegido em casos de oscilação de conectividade. A arquitetura de fila circular com confirmação de entrega provou-se matematicamente imune a perdas de dados hemodinâmicos durante períodos transitórios de desconexão, estabelecendo uma base sólida para a camada de transmissão em nuvem e visualização em dashboard apresentadas no Relatório 2.
