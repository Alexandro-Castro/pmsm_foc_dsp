//#############################################################################
//
// FILE:   encoder.c
//
// TITLE:  Leitura periodica do encoder Tamagawa (T-Format)
//
// Construido sobre a biblioteca PM_tformat da TI (projeto
// PM_tformat_f2837x_lib). A inicializacao do hardware segue o exemplo
// tformat_f2837x_comms_demo do MotorControl SDK; a diferenca e que aqui a
// leitura e feita uma vez por periodo de controle, sem a CPU ficar esperando.
//
// Pinos usados (configurados pelo SysConfig em
// source/tformat_f2837x_boostxl_posmgr_site2.syscfg):
//   GPIO63 (J6-55)  SPISIMOB  <- RO do MAX3485 (resposta do encoder)
//   GPIO64 (J6-54)  SPISOMIB  -> DI do MAX3485 (comando)
//   GPIO65 (J5-47)  SPICLKB   <- fio vindo do GPIO7 (clock gerado pelo CLB)
//   GPIO66 (J6-59)  SPISTEB   -> ligado ao GND
//   GPIO7  (J8-79)  EPWM4B    -> saida de clock do CLB
//   GPIO9  (J8-77)  TxEN      -> DE/RE do MAX3485
//
//#############################################################################

#include "encoder.h"
#include "app_config.h"
#include "device.h"
#include "board.h"                  // gerado pelo SysConfig no build
#include "PM_tformat_include.h"

//-----------------------------------------------------------------------------
// Constantes
//-----------------------------------------------------------------------------
// O CLB divide o seu relogio (EPWMCLK = 100 MHz) para gerar o clock de
// 2,5 MHz do T-Format. O divisor conta meio periodo: 100 / (2 * 2,5) = 20.
#define TFORMAT_CLB_CLOCK_MHZ   (APP_EPWMCLK_HZ / 1000000UL)
#define TFORMAT_BUS_CLOCK_MHZ   2.5f
#define TFORMAT_FREQ_DIVIDER    (TFORMAT_CLB_CLOCK_MHZ /                    \
                                 (uint16_t)(2.0f * TFORMAT_BUS_CLOCK_MHZ))

#define TWO_PI                  6.28318530718f
#define RAD_PER_COUNT           (TWO_PI / (float32_t)ENCODER_COUNTS_PER_REV)
#define DEG_PER_COUNT           (360.0f / (float32_t)ENCODER_COUNTS_PER_REV)
#define RADPS_TO_RPM            (60.0f / TWO_PI)

// Coeficiente do filtro de velocidade: y += alpha * (x - y)
#define SPEED_FILTER_ALPHA      (CONTROL_PERIOD_S /                         \
                                 (ENCODER_SPEED_FILTER_TAU_S + CONTROL_PERIOD_S))

//-----------------------------------------------------------------------------
// Variaveis exigidas pela biblioteca PM_tformat (declaradas como extern em
// PM_tformat_include.h; a aplicacao precisa defini-las)
//-----------------------------------------------------------------------------
PM_tformat_DataStruct tformatData;                       // campos recebidos
uint16_t tformatCRCtable[PM_TFORMAT_CRCTABLE_SIZE];      // tabela do CRC
volatile uint16_t tformatRxData[PM_TFORMAT_FIELDS_MAX];  // bytes do SPI

//-----------------------------------------------------------------------------
// Variaveis do modulo
//-----------------------------------------------------------------------------
Encoder encoder;                          // resultado (visivel no Watch)

static bool     requestPending = false;   // existe um pedido em andamento?
static bool     havePrevious   = false;   // ja houve uma leitura valida?
static uint32_t previousPosition;         // ultima posicao valida
static uint16_t periodsSinceValid;        // periodos desde a ultima valida

//-----------------------------------------------------------------------------
// Funcoes internas
//-----------------------------------------------------------------------------
static void encoder_configEpwm4(void);
static void encoder_configSpi(void);
static void encoder_registerError(void) __attribute__((ramfunc));

// Funcoes de interrupcao nao podem ser "static" (exigencia do compilador)
void encoder_spiRxIsr(void) __attribute__((interrupt("HPI")))
                            __attribute__((ramfunc));

//=============================================================================
// Inicializacao
//=============================================================================
void encoder_init(void)
{
    // 1) ePWM4: no F2837xD o CLB do T-Format (tile 4) usa a saida EPWM4B
    //    (GPIO7) para levar o clock ate o pino SPICLK.
    SysCtl_enablePeripheral(SYSCTL_PERIPH_CLK_EPWM4);
    encoder_configEpwm4();

    // 2) Tabela do CRC. No F2837xD o CRC da resposta e calculado pela CPU
    //    com esta tabela (o CLB deste chip nao calcula CRC).
    PM_tformat_generateCRCTable(PM_TFORMAT_NBITS_POLY, PM_TFORMAT_POLY,
                                tformatCRCtable);

    // 3) CLB: carrega a logica do T-Format e o clock de 2,5 MHz
    PM_tformat_setupPeriph();
    PM_tformat_setFreq(TFORMAT_FREQ_DIVIDER);

    // 4) Pinos e XBARs. Estas funcoes sao geradas pelo SysConfig a partir do
    //    arquivo .syscfg (ficam em <build>/syscfg/board.c).
    GPIO_init();
    PinMux_init();
    CLBXBAR_init();
    INPUTXBAR_init();
    OUTPUTXBAR_init();

    // 5) SPI em modo escravo e interrupcao de recepcao
    encoder_configSpi();
    Interrupt_register(PM_TFORMAT_INT_SPI_RX, &encoder_spiRxIsr);
    Interrupt_enable(PM_TFORMAT_INT_SPI_RX);
    SPI_clearInterruptStatus(PM_TFORMAT_SPI, SPI_INT_RXFF);
    SPI_enableInterrupt(PM_TFORMAT_SPI, SPI_INT_RXFF);
    SPI_enableModule(PM_TFORMAT_SPI);

    // 6) Liga a alimentacao do encoder e espera ele iniciar. O GPIO139
    //    controla a chave de 5 V da BOOSTXL-POSMGR; na montagem com o MAX3485
    //    o encoder ja esta ligado na fonte e este pino nao faz nada.
    GPIO_writePin(TFORMAT_GPIO_PWRCTRL, 1);
    DEVICE_DELAY_US(ENCODER_POWER_ON_DELAY_US);

    encoder = (Encoder){0};
}

//=============================================================================
// Execucao periodica (chamadas pela interrupcao de controle)
//=============================================================================
void encoder_requestPosition(void)
{
    // Carrega o comando ID0 no SPI, reinicia o CLB e zera dataReady.
    PM_tformat_setupCommandReadoutOrReset(PM_TFORMAT_ID0,
                                          PM_TFORMAT_RX_CLOCKS_ID0,
                                          PM_TFORMAT_RX_FIELDS_ID0,
                                          PM_TFORMAT_TX_CLOCKS_ID0,
                                          PM_TFORMAT_FIFO_LEVEL_ID0);

    // Dispara a transacao. Daqui em diante o CLB e o SPI trabalham sozinhos.
    PM_tformat_startOperation();
    requestPending = true;
}

void encoder_update(void)
{
    uint32_t position;
    uint32_t elecCounts;

    // Na primeira chamada ainda nao existe pedido para processar.
    if(!requestPending)
    {
        return;
    }
    requestPending = false;
    periodsSinceValid++;

    // 1) A resposta chegou? (dataReady e ligado pela ISR do SPI)
    if(!tformatData.dataReady)
    {
        encoder.timeoutErrors++;
        encoder_registerError();
        return;
    }

    // 2) Separa os campos recebidos e calcula o CRC esperado
    PM_tformat_receiveDataID0_1_7_8_C();

    // 3) Confere a resposta antes de usar a posicao
    if(tformatData.controlField != PM_TFORMAT_CFID0)
    {
        encoder.idErrors++;
        encoder_registerError();
        return;
    }
    if(tformatData.crcCheck != tformatData.crcField)
    {
        encoder.crcErrors++;
        encoder_registerError();
        return;
    }

    // 4) Leitura valida: monta a posicao a partir dos campos ABS0..ABS2
    position = PM_tformat_updatePositionOrTurns(tformatData.dataField0,
                                                tformatData.dataField1,
                                                tformatData.dataField2);
    position &= ENCODER_COUNTS_MASK;

    encoder.position     = position;
    encoder.status       = tformatData.statusField;
    encoder.angleMech    = (float32_t)position * RAD_PER_COUNT;
    encoder.angleMechDeg = (float32_t)position * DEG_PER_COUNT;

    // 5) Angulo eletrico = (pares de polos * angulo mecanico + offset),
    //    dando a volta em 2*pi. Como 131072 e potencia de 2, a "volta" e
    //    feita com uma mascara nas contagens, sem erro de arredondamento.
    elecCounts = (position * MOTOR_POLE_PAIRS + ENCODER_ELEC_OFFSET_COUNTS)
                 & ENCODER_COUNTS_MASK;
    encoder.angleElec = (float32_t)elecCounts * RAD_PER_COUNT;

    // 6) Velocidade = variacao da posicao / tempo desde a ultima leitura boa
    if(havePrevious)
    {
        int32_t   delta;
        float32_t speedRaw;

        delta = (int32_t)position - (int32_t)previousPosition;

        // Corrige a passagem pelo zero (131071 -> 0 ou 0 -> 131071)
        if(delta > (int32_t)(ENCODER_COUNTS_PER_REV / 2UL))
        {
            delta -= (int32_t)ENCODER_COUNTS_PER_REV;
        }
        else if(delta < -(int32_t)(ENCODER_COUNTS_PER_REV / 2UL))
        {
            delta += (int32_t)ENCODER_COUNTS_PER_REV;
        }

        speedRaw = ((float32_t)delta * RAD_PER_COUNT) /
                   ((float32_t)periodsSinceValid * CONTROL_PERIOD_S);

        // Filtro passa-baixas de 1a ordem
        encoder.speedMech += SPEED_FILTER_ALPHA * (speedRaw - encoder.speedMech);
        encoder.speedRpm   = encoder.speedMech * RADPS_TO_RPM;
    }

    previousPosition          = position;
    havePrevious              = true;
    periodsSinceValid         = 0U;
    encoder.valid             = true;
    encoder.consecutiveErrors = 0U;
    encoder.readCount++;
}

//=============================================================================
// Funcoes internas
//=============================================================================

// Leitura com erro: mantem o ultimo angulo valido e conta a falha.
static void encoder_registerError(void)
{
    encoder.valid = false;

    if(encoder.consecutiveErrors < 0xFFFFU)
    {
        encoder.consecutiveErrors++;
    }
    if(encoder.consecutiveErrors >= ENCODER_MAX_CONSECUTIVE_ERRORS)
    {
        encoder.fault = true;
    }
}

// ISR do SPI: chamada quando o FIFO de recepcao tem todos os campos da
// resposta. Copia os bytes para a biblioteca e avisa que a resposta chegou.
// "HPI" = interrupcao de alta prioridade da FPU (salvamento rapido de
// contexto, nao aninhavel), como no exemplo da TI.
void encoder_spiRxIsr(void)
{
    uint16_t i;

    for(i = 0U; i < tformatData.fifoLevel; i++)
    {
        tformatRxData[i] = SPI_readDataNonBlocking(PM_TFORMAT_SPI) & 0xFFU;
    }

    SPI_clearInterruptStatus(PM_TFORMAT_SPI, SPI_INT_RXFF_OVERFLOW);
    SPI_clearInterruptStatus(PM_TFORMAT_SPI, SPI_INT_RXFF);
    Interrupt_clearACKGroup(INTERRUPT_ACK_GROUP6);
    tformatData.dataReady = true;
}

// Deixa a saida EPWM4B (GPIO7) num nivel definido antes de o CLB assumir o
// pino. Do ePWM4 so e usado o controle do pino; o PWM em si nao roda.
static void encoder_configEpwm4(void)
{
    EPWM_setTripZoneAction(EPWM4_BASE, EPWM_TZ_ACTION_EVENT_TZB,
                           EPWM_TZ_ACTION_HIGH);
    EPWM_forceTripZoneEvent(EPWM4_BASE, EPWM_TZ_FORCE_EVENT_OST);
}

// SPI escravo: o clock vem do CLB (via GPIO7 -> GPIO65), por isso a taxa
// passada aqui (500 kHz) nao define a velocidade real da comunicacao.
static void encoder_configSpi(void)
{
    SPI_disableModule(PM_TFORMAT_SPI);
    SPI_disableLoopback(PM_TFORMAT_SPI);
    SPI_setConfig(PM_TFORMAT_SPI, DEVICE_LSPCLK_FREQ, SPI_PROT_POL1PHA0,
                  SPI_MODE_SLAVE, 500000, PM_TFORMAT_FIFO_WORD_BITS);
    SPI_clearInterruptStatus(PM_TFORMAT_SPI, SPI_INT_RX_OVERRUN |
                             SPI_INT_RX_DATA_TX_EMPTY | SPI_INT_RXFF |
                             SPI_INT_RXFF_OVERFLOW | SPI_INT_TXFF);
    SPI_enableFIFO(PM_TFORMAT_SPI);
    SPI_setFIFOInterruptLevel(PM_TFORMAT_SPI, SPI_FIFO_TX3, SPI_FIFO_RX3);
    SPI_setEmulationMode(PM_TFORMAT_SPI, SPI_EMULATION_FREE_RUN);
    SPI_enableModule(PM_TFORMAT_SPI);
    SPI_resetTxFIFO(PM_TFORMAT_SPI);
    SPI_resetRxFIFO(PM_TFORMAT_SPI);
}
