//#############################################################################
//
// FILE:   pwm.c
//
// TITLE:  PWM trifasico do inversor (ePWM1, ePWM2, ePWM3)
//
// Caminho do sinal dentro de cada ePWM:
//
//   contador -> comparador A -> "action qualifier" -> tempo morto -> trip zone -> pino
//   (0..2500..0)  (CMPA)        sinal S (1 = chave      gera A e B   forca tudo
//                               superior ligada)        complementares  em 0
//
// 1) Contador triangular: 0 -> 2500 -> 0 em 50 us (20 kHz).
// 2) S = 1 enquanto o contador esta acima de CMPA. O pulso fica centrado no
//    pico do contador; com o contador em zero, as tres chaves inferiores estao
//    ligadas (instante certo para medir corrente nos shunts do IRAM, que
//    ficam nos emissores das chaves inferiores).
//      duty = 1 - CMPA / TBPRD   ->   CMPA = TBPRD * (1 - duty)
// 3) Tempo morto (modo "ativo alto complementar"):
//      pino A (superior) = S atrasado na subida        -> liga 1 us depois
//      pino B (inferior) = NAO(S atrasado na descida)  -> liga 1 us depois
//    Durante o tempo morto os dois pinos ficam em 0 (as duas chaves desligadas).
// 4) Trip zone: quando ativo, forca os dois pinos em 0, por cima de tudo.
//
// Protecoes do proprio modulo IRAMX16UP60A (alem das deste arquivo): logica
// que impede ligar as duas chaves do braco juntas, tempo morto interno de
// ~290 ns e desligamento pelo pino T/ITRIP (usado pelo sinal Ativa).
//
//#############################################################################

#include "pwm.h"
#include "app_config.h"
#include "device.h"

//-----------------------------------------------------------------------------
// Tabelas dos tres bracos (indice 0 = fase A, 1 = B, 2 = C)
//-----------------------------------------------------------------------------
static const uint32_t pwmBase[3] = {EPWM1_BASE, EPWM2_BASE, EPWM3_BASE};

// Pinos: superior e inferior de cada fase
static const uint32_t pwmGpio[6] = {0U, 1U, 2U, 3U, 4U, 5U};

// Funcao do pino como GPIO comum (usada so durante a inicializacao)
static const uint32_t pwmGpioCfg[6] = {GPIO_0_GPIO0, GPIO_1_GPIO1,
                                       GPIO_2_GPIO2, GPIO_3_GPIO3,
                                       GPIO_4_GPIO4, GPIO_5_GPIO5};

// Funcao do pino como saida do ePWM
static const uint32_t pwmEpwmCfg[6] = {GPIO_0_EPWM1A, GPIO_1_EPWM1B,
                                       GPIO_2_EPWM2A, GPIO_3_EPWM2B,
                                       GPIO_4_EPWM3A, GPIO_5_EPWM3B};

// O pino Falha chega aos ePWM pelo Input X-BAR. A entrada INPUT1 ja e usada
// pelo encoder (GPIO63); INPUT2 vai para o sinal TZ2 dos ePWM.
#define PWM_FAULT_XBAR_INPUT    XBAR_INPUT2

//-----------------------------------------------------------------------------
// Variaveis do modulo
//-----------------------------------------------------------------------------
PwmStatus pwm;                             // estado (visivel no Watch)

//-----------------------------------------------------------------------------
// Funcoes internas
//-----------------------------------------------------------------------------
static void pwm_configBoardSignals(void);
static void pwm_configTimeBase(uint16_t phase);
static void pwm_configOutputs(uint16_t phase);
static void pwm_configTripZone(uint16_t phase);
static uint16_t pwm_dutyToCmpa(float32_t duty) __attribute__((ramfunc));

//=============================================================================
// Inicializacao
//=============================================================================
void pwm_init(void)
{
    uint16_t i;

    // 1) Antes de tudo, os 6 pinos viram saidas em 0 (chaves desligadas) e o
    //    modulo IRAM fica desabilitado (Ativa = 0)
    for(i = 0U; i < 6U; i++)
    {
        GPIO_setPadConfig(pwmGpio[i], GPIO_PIN_TYPE_STD);
        GPIO_writePin(pwmGpio[i], 0U);
        GPIO_setDirectionMode(pwmGpio[i], GPIO_DIR_MODE_OUT);
        GPIO_setPinConfig(pwmGpioCfg[i]);
    }
    pwm_configBoardSignals();

    // 2) Configura os tres ePWM com o trip zone ativo (saidas em 0)
    for(i = 0U; i < 3U; i++)
    {
        pwm_configTripZone(i);
        pwm_configTimeBase(i);
        pwm_configOutputs(i);
    }

    // 3) So agora os pinos passam para o ePWM. Como o trip zone ja esta
    //    ativo, eles continuam em 0.
    for(i = 0U; i < 6U; i++)
    {
        GPIO_setPinConfig(pwmEpwmCfg[i]);
    }

    pwm = (PwmStatus){0};
    for(i = 0U; i < 3U; i++)
    {
        pwm.duty[i] = 0.5f;
        pwm.cmpa[i] = pwm_dutyToCmpa(0.5f);
    }
}

void pwm_startTimeBase(void)
{
    uint16_t i;

    // O ePWM1 manda um pulso de sincronismo a cada zero do seu contador; o
    // ePWM2 e o ePWM3 zeram o contador nesse instante. A partir do primeiro
    // periodo os tres bracos ficam alinhados.
    for(i = 0U; i < 3U; i++)
    {
        EPWM_setTimeBaseCounterMode(pwmBase[i], EPWM_COUNTER_MODE_UP_DOWN);
    }
}

//=============================================================================
// Liga / desliga
//=============================================================================
void pwm_enable(void)
{
    uint16_t i;

    // Comeca com 50 % nas tres fases: tensao media entre fases igual a zero
    pwm_setDuty(0.5f, 0.5f, 0.5f);

    for(i = 0U; i < 3U; i++)
    {
        // Limpa as causas registradas e libera o trip zone. Se a causa
        // continuar presente (por exemplo, Falha = 1), o trip volta na hora
        // e pwm_checkTrip() desliga tudo de novo.
        EPWM_clearOneShotTripZoneFlag(pwmBase[i], 0xFFU);
        EPWM_clearTripZoneFlag(pwmBase[i],
                               EPWM_TZ_INTERRUPT | EPWM_TZ_FLAG_OST);
    }

    // Habilita o modulo IRAM
    GPIO_writePin(INV_ENABLE_GPIO, 1U);

    pwm.enabled = true;
}

void pwm_disable(void)
{
    uint16_t i;

    pwm.enabled = false;

    for(i = 0U; i < 3U; i++)
    {
        EPWM_forceTripZoneEvent(pwmBase[i], EPWM_TZ_FORCE_EVENT_OST);
    }

    // Desabilita o modulo IRAM (segundo caminho de desligamento)
    GPIO_writePin(INV_ENABLE_GPIO, 0U);
}

bool pwm_checkTrip(void)
{
    uint16_t i;
    bool     tripped = false;

    if(!pwm.enabled)
    {
        return false;
    }

    for(i = 0U; i < 3U; i++)
    {
        if((EPWM_getTripZoneFlagStatus(pwmBase[i]) & EPWM_TZ_FLAG_OST) != 0U)
        {
            tripped = true;
        }
    }

    if(tripped)
    {
        // Registra a causa e garante os tres bracos desligados, mesmo que so
        // um tenha disparado
        pwm.tripSource = EPWM_getOneShotTripZoneFlagStatus(EPWM1_BASE);
        pwm.tripCount++;
        pwm_disable();
    }

    return tripped;
}

//=============================================================================
// Razao ciclica
//=============================================================================
void pwm_setDuty(float32_t dutyA, float32_t dutyB, float32_t dutyC)
{
    uint16_t i;

    pwm.duty[0] = dutyA;
    pwm.duty[1] = dutyB;
    pwm.duty[2] = dutyC;

    for(i = 0U; i < 3U; i++)
    {
        pwm.cmpa[i] = pwm_dutyToCmpa(pwm.duty[i]);
        EPWM_setCounterCompareValue(pwmBase[i], EPWM_COUNTER_COMPARE_A,
                                    pwm.cmpa[i]);
    }
}

//=============================================================================
// Funcoes internas
//=============================================================================

// duty (0..1) -> CMPA. O resultado fica entre 1 e TBPRD-1 para evitar os
// casos de borda do comparador (CMPA = 0 ou CMPA = TBPRD). A diferenca e de
// 10 ns, menor que o tempo morto.
static uint16_t pwm_dutyToCmpa(float32_t duty)
{
    float32_t cmpa;

    if(duty < 0.0f)
    {
        duty = 0.0f;
    }
    else if(duty > 1.0f)
    {
        duty = 1.0f;
    }

    cmpa = (float32_t)PWM_TBPRD * (1.0f - duty);

    if(cmpa < 1.0f)
    {
        cmpa = 1.0f;
    }
    else if(cmpa > (float32_t)(PWM_TBPRD - 1U))
    {
        cmpa = (float32_t)(PWM_TBPRD - 1U);
    }

    return (uint16_t)(cmpa + 0.5f);
}

// Sinais Ativa e Falha do conector JP1
static void pwm_configBoardSignals(void)
{
    // Ativa: saida em 0 (modulo desabilitado). Com o pino em 0 ou solto, o
    // LED do optoacoplador fica apagado e a placa poe o T/ITRIP do IRAM em
    // nivel alto, o que desliga as seis chaves.
    GPIO_setPadConfig(INV_ENABLE_GPIO, GPIO_PIN_TYPE_STD);
    GPIO_writePin(INV_ENABLE_GPIO, 0U);
    GPIO_setDirectionMode(INV_ENABLE_GPIO, GPIO_DIR_MODE_OUT);
    GPIO_setPinConfig(INV_ENABLE_GPIO_CFG);

    // Falha: entrada com pull-up (a saida do optoacoplador e coletor
    // aberto). Pino em 1 = falha ou placa sem alimentacao. Fio solto tambem
    // le 1, entao a falta da ligacao impede ligar o PWM.
    //
    // O trip zone dispara com o sinal em 0, por isso a entrada e invertida
    // (GPIO_PIN_TYPE_INVERT). O filtro de 6 amostras a cada 0,2 us ignora
    // pulsos de ruido menores que ~1 us.
    GPIO_setPinConfig(INV_FAULT_GPIO_CFG);
    GPIO_setDirectionMode(INV_FAULT_GPIO, GPIO_DIR_MODE_IN);
    GPIO_setPadConfig(INV_FAULT_GPIO,
                      GPIO_PIN_TYPE_PULLUP | GPIO_PIN_TYPE_INVERT);
    GPIO_setQualificationPeriod(INV_FAULT_GPIO, 40U);   // 40 / 200 MHz
    GPIO_setQualificationMode(INV_FAULT_GPIO, GPIO_QUAL_6SAMPLE);

    XBAR_setInputPin(PWM_FAULT_XBAR_INPUT, INV_FAULT_GPIO);
}

// Contador triangular de 20 kHz, parado ate pwm_startTimeBase()
static void pwm_configTimeBase(uint16_t phase)
{
    uint32_t base = pwmBase[phase];

    EPWM_setTimeBaseCounterMode(base, EPWM_COUNTER_MODE_STOP_FREEZE);
    EPWM_setClockPrescaler(base, EPWM_CLOCK_DIVIDER_1, EPWM_HSCLOCK_DIVIDER_1);
    EPWM_setTimeBasePeriod(base, PWM_TBPRD);
    EPWM_setTimeBaseCounter(base, 0U);

    // Ao pausar no depurador, o contador termina o periodo e para (as saidas
    // ja foram desligadas pelo trip zone, veja pwm_configTripZone)
    EPWM_setEmulationMode(base, EPWM_EMULATION_STOP_AFTER_FULL_CYCLE);

    if(phase == 0U)
    {
        // ePWM1 e o mestre: gera o sincronismo no zero do contador
        EPWM_disablePhaseShiftLoad(base);
        EPWM_setSyncOutPulseMode(base, EPWM_SYNC_OUT_PULSE_ON_COUNTER_ZERO);
    }
    else
    {
        // ePWM2 e ePWM3: zeram o contador no sincronismo e repassam o pulso
        // para o proximo ePWM da cadeia (ePWM1 -> ePWM2 -> ePWM3)
        EPWM_enablePhaseShiftLoad(base);
        EPWM_setPhaseShift(base, 0U);
        EPWM_setCountModeAfterSync(base, EPWM_COUNT_MODE_UP_AFTER_SYNC);
        EPWM_setSyncOutPulseMode(base, EPWM_SYNC_OUT_PULSE_ON_EPWMxSYNCIN);
    }
}

// Comparador, action qualifier e tempo morto
static void pwm_configOutputs(uint16_t phase)
{
    uint32_t base = pwmBase[phase];

    // CMPA com registrador "sombra": o valor escrito na interrupcao so passa
    // a valer no proximo zero do contador (pulso sempre simetrico)
    EPWM_setCounterCompareShadowLoadMode(base, EPWM_COUNTER_COMPARE_A,
                                         EPWM_COMP_LOAD_ON_CNTR_ZERO);
    EPWM_setCounterCompareValue(base, EPWM_COUNTER_COMPARE_A,
                                pwm_dutyToCmpa(0.5f));

    // Sinal S: sobe quando o contador passa por CMPA subindo e desce quando
    // passa por CMPA descendo -> S = 1 com o contador acima de CMPA
    EPWM_setActionQualifierAction(base, EPWM_AQ_OUTPUT_A,
                                  EPWM_AQ_OUTPUT_HIGH,
                                  EPWM_AQ_OUTPUT_ON_TIMEBASE_UP_CMPA);
    EPWM_setActionQualifierAction(base, EPWM_AQ_OUTPUT_A,
                                  EPWM_AQ_OUTPUT_LOW,
                                  EPWM_AQ_OUTPUT_ON_TIMEBASE_DOWN_CMPA);

    // Tempo morto: os dois pinos sao gerados a partir de S
    EPWM_setDeadBandCounterClock(base, EPWM_DB_COUNTER_CLOCK_FULL_CYCLE);
    EPWM_setRisingEdgeDeadBandDelayInput(base, EPWM_DB_INPUT_EPWMA);
    EPWM_setFallingEdgeDeadBandDelayInput(base, EPWM_DB_INPUT_EPWMA);
    EPWM_setDeadBandDelayMode(base, EPWM_DB_RED, true);
    EPWM_setDeadBandDelayMode(base, EPWM_DB_FED, true);
    EPWM_setRisingEdgeDelayCount(base, PWM_DEADTIME_COUNTS);
    EPWM_setFallingEdgeDelayCount(base, PWM_DEADTIME_COUNTS);

    // "Ativo alto complementar": pino A = subida atrasada (sem inverter),
    // pino B = NAO(descida atrasada)
    EPWM_setDeadBandDelayPolarity(base, EPWM_DB_RED,
                                  EPWM_DB_POLARITY_ACTIVE_HIGH);
    EPWM_setDeadBandDelayPolarity(base, EPWM_DB_FED,
                                  EPWM_DB_POLARITY_ACTIVE_LOW);
}

// Trip zone: forca os dois pinos em 0 (chaves desligadas)
static void pwm_configTripZone(uint16_t phase)
{
    uint32_t base = pwmBase[phase];

    EPWM_setTripZoneAction(base, EPWM_TZ_ACTION_EVENT_TZA,
                           EPWM_TZ_ACTION_LOW);
    EPWM_setTripZoneAction(base, EPWM_TZ_ACTION_EVENT_TZB,
                           EPWM_TZ_ACTION_LOW);

    // Fontes que desligam o PWM sozinhas (sem depender do software):
    //   TZ2 = pino Falha da placa inversora em 1
    //   TZ5 = falha do relogio do sistema
    //   TZ6 = CPU parada pelo depurador (pause ou breakpoint no CCS)
    EPWM_enableTripZoneSignals(base, EPWM_TZ_SIGNAL_OSHT2 |
                                     EPWM_TZ_SIGNAL_OSHT5 |
                                     EPWM_TZ_SIGNAL_OSHT6);

    // Comeca desligado
    EPWM_forceTripZoneEvent(base, EPWM_TZ_FORCE_EVENT_OST);
}
