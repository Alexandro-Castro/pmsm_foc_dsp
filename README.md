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
| `PM_tformat_f2837x_lib` | Biblioteca T-Format da TI (CLB + SPI), copiada do MotorControl SDK 5.04 |
| `tformat_f2837x_comms_demo` | Exemplo de comunicação da TI (TIDM-1011), build configuration `boostxl_posmgr_site2_RAM` |

Origem: `C:\ti\C2000Ware_MotorControl_SDK_5_04_00_00\solutions\boostxl_posmgr\f2837xd\ccs\tformat\tformat_systemtest.projectspec`.

Alteração em relação ao exemplo da TI: em `include/tformat.h`, os comandos que gravam a
EEPROM do encoder (IDD/ID6) e os resets (ID7, ID8, IDC) foram desligados. O exemplo envia
somente leituras (ID0, ID1, ID2, ID3).

## Compilar e rodar (CCS 21)

1. Abrir esta pasta como workspace no CCS. O exemplo procura a biblioteca em
   `${CCS_WORKSPACE_DIR}/PM_tformat_f2837x_lib`.
2. Build configuration de `tformat_f2837x_comms_demo`: `boostxl_posmgr_site2_RAM`.
3. **Project → Build Project(s)**. A biblioteca é compilada junto.
4. **F5** e depois **Continue**. No Watch: `encoderData`, `errorData`, `tformatSpiRxIsrTicker`.

## Resultado validado

- `encoderData.position` de 0 a 131 071 por volta (17 bits), `turns` muda a cada volta completa, `errorData` em zero.
- Exemplo de leitura: `{"position":1457,"turns":1,"id":17}` → ângulo mecânico = 1457 × 360° / 131 072 ≈ 4,00°.
