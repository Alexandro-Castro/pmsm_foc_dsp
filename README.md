# Leitura do encoder Tamagawa (T-Format) com o LAUNCHXL-F28379D

Base de firmware para o acionamento do PMSM de 0,75 kW (FOC/MPC). Esta versão lê o
encoder absoluto do motor pelo protocolo Tamagawa T-Format usando a biblioteca
`PM_tformat` da TI (referência TIDM-1011).

## Hardware

| Item | Detalhe |
|---|---|
| DSP | LAUNCHXL-F28379D (TMS320F28379D), depuração pela XDS100v2 da placa |
| Encoder | Tamagawa TS5669N1138: absoluto, 17 bits/volta + 16 bits multivoltas, T-Format 2,5 Mbps sobre RS-485 |
| Interface RS-485 | Módulo MAX3485 (3,3 V), no lugar da BOOSTXL-POSMGR |
| Alimentação do encoder | Fonte de bancada 5 V com limite de corrente, GND comum com a LaunchPad |
| Inversor | Placa trifásica usada no TCC com o ESP32 ([controle-MIT-V-F-ESP32-MCPWM](https://github.com/Alexandro-Castro/controle-MIT-V-F-ESP32-MCPWM)): módulo IRAMX16UP60A (16 A, 600 V) com shunts de 0,1 Ω, entradas isoladas por optoacopladores HCPL-2611, proteção por hardware (corrente e tensão, com botão de rearme), entrada "Ativa" e saída "Falha" |

## Ligações (site 2 da LaunchPad)

Pinagem validada em bancada. Ver também [docs/pinos_tformat_MAX3485.png](docs/pinos_tformat_MAX3485.png).

| LaunchPad | GPIO / função | Liga em |
|---|---|---|
| J5-41 | 3V3 | VCC do módulo |
| J6-60 | GND | GND do módulo |
| J6-54 | GPIO64, SPISOMIB (comando) | pino **RX** do módulo (DI, pino 4 do MAX3485) |
| J6-55 | GPIO63, SPISIMOB (resposta) | pino **TX** do módulo (RO, pino 1 do MAX3485) |
| J8-77 | GPIO9, TxEN (OUTPUTXBAR6) | DE + RE do módulo |
| J8-79 | GPIO7, clock do CLB (EPWM4B) | fio até J5-47 |
| J5-47 | GPIO65, SPICLKB | fio vindo de J8-79 |
| J6-59 | GPIO66, SPISTEB | GND (fio até J6-60) |

| Fio do encoder | Função | Liga em |
|---|---|---|
| Azul | SD | A do módulo |
| Azul/preto | /SD | B do módulo |
| Vermelho | Vcc | +5 V da fonte |
| Preto, marrom/preto | GND | GND comum |
| Marrom | VB (bateria) | não ligado |

Pontos que causaram falhas na montagem:

- **J8-79 ↔ J5-47 é obrigatório no F2837xD.** O CLB gera o clock do SPI num GPIO e ele precisa voltar por fora para o pino SPICLK.
- **J6-59 (STE) precisa estar no GND.** Com STE em 1, o SPI escravo ignora o clock e não transmite.
- **O módulo nomeia os pinos pelo lado dele:** "RX" do módulo = DI (entrada) e "TX" = RO (saída). O TX do DSP vai no RX do módulo.

## Projetos

| Projeto | Conteúdo |
|---|---|
| `PM_tformat_f2837x_lib` | Biblioteca T-Format da TI (CLB + SPI), copiada do MotorControl SDK 5.04 sem alterações |
| `tformat_f2837x_comms_demo` | Firmware do acionamento. Começou como o exemplo de comunicação da TI (TIDM-1011) e está sendo transformado no FOC |

Origem do exemplo: `C:\ti\C2000Ware_MotorControl_SDK_5_04_00_00\solutions\boostxl_posmgr\f2837xd\ccs\tformat\tformat_systemtest.projectspec`.

### Estrutura do firmware (`tformat_f2837x_comms_demo`)

| Arquivo | Função |
|---|---|
| `include/app_config.h` | Parâmetros ajustáveis: frequência de controle, encoder, motor |
| `source/main.c` | Inicialização; depois disso tudo roda nas interrupções |
| `source/pwm.c`, `include/pwm.h` | PWM trifásico do inversor (ePWM1–3), tempo morto e proteção (trip zone) |
| `source/control.c`, `include/control.h` | Interrupção de controle de 20 kHz (início do período do ePWM1) |
| `source/encoder.c`, `include/encoder.h` | Leitura do encoder (comando ID0), ângulos, velocidade e diagnóstico |
| `source/tformat_f2837x_boostxl_posmgr_site2.syscfg` | Pinos, XBARs e CLB do T-Format (SysConfig) |

Recursos do chip ocupados pelo encoder: CLB tile 4 + ePWM4 (GPIO7), SPIB (GPIO63–66) e
OUTPUTXBAR6 (GPIO9). O PWM do inversor usa ePWM1–ePWM3 (GPIO0–5).

Na configuração RAM, todo o código vai para a seção `.TI.ramfunc`, e cada arquivo `.c` vira um
bloco único. Por isso, o arquivo de linker `cmd/pm_tformat_2837x_ram_lnk_cpu1.cmd` junta LS1–LS4
(blocos contíguos) numa região só, `RAMLS1_4`: o código de `pwm.c` passa de 0x800 palavras.

## Etapa 1: leitura periódica do encoder a 20 kHz

A cada 50 µs (frequência de chaveamento do inversor), a interrupção do ePWM1:

1. processa a resposta do encoder pedida no período anterior (`encoder_update`);
2. pede a próxima posição (`encoder_requestPosition`).

A transação (comando + resposta, ~32 µs) é feita pelo CLB e pelo SPI sem a CPU esperar.
O ângulo usado em cada período foi medido pelo encoder ~46 µs antes.

| Medida no osciloscópio | Esperado |
|---|---|
| J8-78 (GPIO8) | Pulso a cada 50 µs; a largura é a duração da interrupção |
| J8-77 (TxEN) | Pulso de ~4,4 µs logo depois da subida de J8-78 |
| LED azul (LD10) | Pisca a cada 0,5 s |

Variáveis para o Watch: `encoder` e `control`.

Pendências em `app_config.h`: `MOTOR_POLE_PAIRS` (valor provisório 1) e
`ENCODER_ELEC_OFFSET_COUNTS` (medido na etapa 4). Sem eles, `encoder.angleElec` ainda não
corresponde ao ângulo elétrico real.

## Etapa 2: PWM do inversor

PWM simétrico de 20 kHz com tempo morto de 1 µs. A placa inversora segue o esquemático
`Inversor.pdf` (2019), com o módulo IRAMX16UP60A (16 A, 600 V).

### Ligação LaunchPad ↔ conector JP1 da placa

| JP1 | Sinal | Na placa vai para | LaunchPad |
|---|---|---|---|
| 16 | PWM0 | HIN1 (fase A, chave superior) | J4-40, GPIO0 (EPWM1A) |
| 14 | PWM1 | LIN1 (fase A, chave inferior) | J4-39, GPIO1 (EPWM1B) |
| 12 | PWM2 | HIN2 (fase B, superior) | J4-38, GPIO2 (EPWM2A) |
| 10 | PWM3 | LIN2 (fase B, inferior) | J4-37, GPIO3 (EPWM2B) |
| 8 | PWM4 | HIN3 (fase C, superior) | J4-36, GPIO4 (EPWM3A) |
| 6 | PWM5 | LIN3 (fase C, inferior) | J4-35, GPIO5 (EPWM3B) |
| 4 | Ativa | T/ITRIP do IRAM (via Opto8) | J4-34, GPIO24 |
| 2 | Falha | saída do Opto1 (coletor aberto) | J4-33, GPIO16 |
| 1 | VCC do Opto1 | — | J3-21 (5 V) |
| 3, 5, …, 15 | GND | — | J3-22 (GND) |

### Lógica

Pino do DSP em **1** → LED do optoacoplador aceso (220 Ω) → saída do HCPL-2611 em 0 → entrada
HIN/LIN do IRAM (ativa em 0) → **chave ligada**. Pino em 0 ou solto → chave desligada. O firmware
usa o tempo morto "ativo alto complementar": no tempo morto e no estado seguro, os pinos ficam em 0.

"Ativa" em 0 ou solto → a placa põe o T/ITRIP do IRAM acima de 4,3 V (R16, R18, DZ1) → as seis
chaves desligam. Com o DSP em reset, sem alimentação ou com o cabo solto, a placa fica desligada.
Depois que "Ativa" sobe, o IRAM leva ~8 ms para sair do estado de falha.

> O TCC concluiu que a placa só inverte o sinal e por isso usou "ativo baixo complementar" no
> ESP32. Como as entradas do IRAM também são ativas em 0, esse modo pedia as duas chaves do braço
> ligadas durante o tempo morto; quem evitava o curto era a lógica de proteção interna do IRAM.

Proteções em camadas:

| Onde | O quê |
|---|---|
| Placa | Sobrecorrente (shunts + TL084 + LM311, ajuste no trimpot "Limite") e tensão, travadas em flip-flop até apertar S1. A falha desliga as entradas dos optoacopladores, o T/ITRIP e acende o LED |
| IRAM | Impede as duas chaves do braço ligadas juntas, tempo morto interno de ~290 ns, subtensão, T/ITRIP |
| DSP (trip zone) | TZ2 = Falha da placa, TZ5 = falha de relógio, TZ6 = CPU parada no CCS, e `pwm_disable()`. Todas zeram os 6 pinos e "Ativa" |

Depois de um desligamento, o PWM só volta escrevendo `pwmTest.enable = 1` de novo.

### Corrente nos LEDs dos optoacopladores

Com 220 Ω em série, cada pino do DSP fornece ~6 mA. Os pinos do F28379D são especificados para
4 mA, e o HCPL-2611 pede 6,3 mA ou mais. Para os testes sem barramento CC dá para ligar direto.
Para acionar o motor, usar um buffer SN74LVC244A alimentado em 3,3 V, com resistores de 10 kΩ para
o GND nas entradas: com o DSP em reset, as saídas do buffer ficam em 0 (chaves desligadas).

### Medição de corrente (para a etapa 3)

As saídas I1, I2 e I3 da placa valem −10 × a tensão no shunt de 0,1 Ω (−1 V/A), são filtradas em
~160 Hz (10 kΩ ∥ 100 nF), vão de −15 V a +15 V e são referenciadas ao negativo do barramento.
Foram feitas para a proteção e não servem direto para o ADC nem para o FOC.

### Teste 1: só a LaunchPad

Ligue J4-33 (Falha) ao GND, para simular "placa sem falha". Sem isso o PWM não liga.

1. Build, F5, Continue. No Watch: `pwm`, `pwmTest`, `encoder`, `control`.
2. Osciloscópio em J4-40 (A, superior) e J4-39 (A, inferior): os dois em 0. J4-34 (Ativa) em 0.
3. `pwmTest.enable = 1`: Ativa vai a 1; PWM de 20 kHz a 50 %. Em cada troca, os dois ficam em 0
   por 1 µs (tempo morto).
4. `pwmTest.duty[0] = 0.25`: J4-40 fica em 1 por ~11,5 µs (25 % de 50 µs menos o tempo morto) e
   J4-39 em 1 por ~36,5 µs. O pulso de J4-40 fica no meio do período, entre dois pulsos de J8-78.
5. Fases B (J4-38/37) e C (J4-36/35) alinhadas com a fase A.
6. Pausar no CCS: os 6 pinos vão para 0 na hora. Ao continuar: `pwm.enabled = 0`,
   `pwmTest.enable = 0`, Ativa em 0 e `pwm.tripSource = 0x20` (CPU parada).
7. Com o PWM ligado, soltar J4-33 do GND: tudo desliga e `pwm.tripSource = 0x02` (Falha).
8. O encoder continua sem erros.

### Teste 2: com a placa, sem barramento CC

Só a alimentação de 127 V da placa (lógica); barramento CC desconectado. Se o LED "Corrente" ou
"Tensão" acender ao ligar, apertar S1.

1. Com o PWM desligado: HIN1 e LIN1 do IRAM (pinos 15 e 18) em ~5 V, T/ITRIP (pino 21) acima de 4,3 V.
2. `pwmTest.enable = 1`: T/ITRIP cai para ~1 V (termistor interno de 100 kΩ com os 6,8 kΩ da
   placa, a 25 °C). HIN1 fica em 0 enquanto J4-40 está em 1, e LIN1 em 0 enquanto J4-39 está em 1.
3. Apertar S1 com o PWM ligado: a placa sinaliza falha e `pwm.tripSource = 0x02`.

## Compilar e rodar (CCS 21)

1. Abrir esta pasta como workspace no CCS. O projeto procura a biblioteca em
   `${CCS_WORKSPACE_DIR}/PM_tformat_f2837x_lib`.
2. Build configuration de `tformat_f2837x_comms_demo`: `boostxl_posmgr_site2_RAM`.
3. **Project → Build Project(s)**. A biblioteca é compilada junto.
4. **F5** e depois **Continue**. No Watch: `encoder`, `control` (com **Continuous Refresh**).

## Resultados validados

- Exemplo da TI: posição de 0 a 131 071 por volta (17 bits), `turns` muda a cada volta completa,
  sem erros. Leitura `{"position":1457,"turns":1,"id":17}` → ângulo mecânico ≈ 4,00°.
- Tempo de uma leitura ID0 medido no osciloscópio: comando 4,4 µs, encoder responde após ~3,2 µs,
  resposta 24 µs (total ~32 µs no barramento).
- Etapa 1 (leitura a 20 kHz): ~1,8 milhão de leituras sem nenhum erro de timeout, ID ou CRC;
  interrupção de controle com 3,4 µs (cerca de 7% do período de 50 µs); ângulo acompanha o eixo
  e a velocidade troca de sinal com o sentido de giro.
- `encoder.status` = 4 (campo SF diferente de zero) com a bateria (VB) desligada. Não afeta a
  posição dentro da volta; o significado do bit ainda será confirmado com o comando ID3 (campo ALMC).
- No Watch, cada campo é lido num instante diferente (a XDS100 é lenta). Com o eixo girando,
  campos da mesma estrutura parecem não bater; para comparar valores, pause o programa.
- Etapa 2 (PWM, só a LaunchPad): 20 kHz, tempo morto de ~1 µs entre J4-40 e J4-39, razão cíclica
  conforme `pwmTest.duty`, fases alinhadas. Pausar no CCS e soltar o pino Falha desligam o PWM
  (`tripSource` 0x20 e 0x02). O encoder segue sem erros. Atenção: ligar `pwmTest.enable` com o
  pino Falha solto faz o PWM desligar e o `enable` voltar a 0; com Falha no GND, escrever 1 de novo.
