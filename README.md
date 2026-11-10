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
| `source/control.c`, `include/control.h` | Interrupção de controle de 20 kHz (base de tempo do ePWM1) |
| `source/encoder.c`, `include/encoder.h` | Leitura do encoder (comando ID0), ângulos, velocidade e diagnóstico |
| `source/tformat_f2837x_boostxl_posmgr_site2.syscfg` | Pinos, XBARs e CLB do T-Format (SysConfig) |

Recursos do chip ocupados pelo encoder: CLB tile 4 + ePWM4 (GPIO7), SPIB (GPIO63–66) e
OUTPUTXBAR6 (GPIO9). O PWM do inversor deve usar ePWM1–ePWM3.

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
