//#############################################################################
//
// FILE:   control.c
//
// TITLE:  Interrupcao de controle de 20 kHz
//
// Linha do tempo de um periodo (50 us):
//
//   t = 0       contador do ePWM1 em zero -> control_isr()
//                 1. encoder_update()          usa a posicao pedida antes
//                 2. encoder_requestPosition() pede a proxima posicao
//                 3. (proximas etapas) ADC -> FOC -> PWM
//   t ~ 32 us   resposta do encoder pronta (ISR do SPI)
//   t = 50 us   proximo periodo
//
//#############################################################################

#include "control.h"
#include "encoder.h"
#include "app_config.h"
#include "device.h"

//-----------------------------------------------------------------------------
// Constantes
//-----------------------------------------------------------------------------
#define TBCLK_TICKS_PER_US      ((float32_t)(APP_EPWMCLK_HZ / 1000000UL))

//-----------------------------------------------------------------------------
// Variaveis do modulo
//-----------------------------------------------------------------------------
ControlStatus control;                    // estado (visivel no Watch)

static uint16_t heartbeatTicks = 0U;      // contador para piscar o LED

//-----------------------------------------------------------------------------
// Funcoes internas
//-----------------------------------------------------------------------------
static void control_measureIsrTime(void) __attribute__((ramfunc));

// Funcoes de interrupcao nao podem ser "static" (exigencia do compilador)
__interrupt void control_isr(void);

//=============================================================================
// Inicializacao
//=============================================================================
void control_init(void)
{
    // 1) Pino de diagnostico (GPIO8 = J8-78): 1 durante a ISR
    GPIO_setPinConfig(CONTROL_DEBUG_GPIO_CFG);
    GPIO_setPadConfig(CONTROL_DEBUG_GPIO, GPIO_PIN_TYPE_STD);
    GPIO_writePin(CONTROL_DEBUG_GPIO, 0U);
    GPIO_setDirectionMode(CONTROL_DEBUG_GPIO, GPIO_DIR_MODE_OUT);

    // 2) LED azul da LaunchPad (GPIO31): pisca para mostrar que a ISR roda
    GPIO_setPinConfig(DEVICE_GPIO_CFG_LED1);
    GPIO_setPadConfig(DEVICE_GPIO_PIN_LED1, GPIO_PIN_TYPE_STD);
    GPIO_writePin(DEVICE_GPIO_PIN_LED1, 1U);          // LED aceso em 0
    GPIO_setDirectionMode(DEVICE_GPIO_PIN_LED1, GPIO_DIR_MODE_OUT);

    // 3) ePWM1 como base de tempo da malha de controle
    //    TBCLK = EPWMCLK / 1 / 1 = 100 MHz (cada contagem = 10 ns)
    //    Contagem triangular 0 -> 2500 -> 0 = 5000 contagens = 50 us
    EPWM_setTimeBaseCounterMode(EPWM1_BASE, EPWM_COUNTER_MODE_STOP_FREEZE);
    EPWM_setClockPrescaler(EPWM1_BASE, EPWM_CLOCK_DIVIDER_1,
                           EPWM_HSCLOCK_DIVIDER_1);
    EPWM_setTimeBasePeriod(EPWM1_BASE, CONTROL_EPWM_TBPRD);
    EPWM_setTimeBaseCounter(EPWM1_BASE, 0U);
    EPWM_disablePhaseShiftLoad(EPWM1_BASE);

    // Ao pausar no depurador, o contador termina o periodo e para. Isso
    // evita que o PWM continue chaveando com o programa parado (importante
    // quando o inversor estiver ligado).
    EPWM_setEmulationMode(EPWM1_BASE, EPWM_EMULATION_STOP_AFTER_FULL_CYCLE);

    // 4) Interrupcao no inicio de cada periodo (contador em zero)
    EPWM_setInterruptSource(EPWM1_BASE, EPWM_INT_TBCTR_ZERO);
    EPWM_setInterruptEventCount(EPWM1_BASE, 1U);
    EPWM_enableInterrupt(EPWM1_BASE);
    Interrupt_register(INT_EPWM1, &control_isr);

    control = (ControlStatus){0};
}

void control_start(void)
{
    EPWM_clearEventTriggerInterruptFlag(EPWM1_BASE);
    Interrupt_enable(INT_EPWM1);

    // Liga o contador: a partir daqui a ISR roda a cada 50 us
    EPWM_setTimeBaseCounterMode(EPWM1_BASE, EPWM_COUNTER_MODE_UP_DOWN);
}

//=============================================================================
// Interrupcao de controle (20 kHz)
//=============================================================================
__interrupt void control_isr(void)
{
    GPIO_writePin(CONTROL_DEBUG_GPIO, 1U);

    // 1) Encoder: processa a resposta do pedido feito no periodo anterior
    encoder_update();

    // 2) Encoder: pede a posicao que sera usada no proximo periodo
    encoder_requestPosition();

    // 3) Proximas etapas: leitura do ADC, FOC e atualizacao do PWM

    // 4) LED pisca a cada 0,5 s
    heartbeatTicks++;
    if(heartbeatTicks >= CONTROL_HEARTBEAT_TICKS)
    {
        heartbeatTicks = 0U;
        GPIO_togglePin(DEVICE_GPIO_PIN_LED1);
    }

    control.isrCount++;
    control_measureIsrTime();

    // 5) Libera a proxima interrupcao do ePWM1 (grupo 3 do PIE)
    EPWM_clearEventTriggerInterruptFlag(EPWM1_BASE);
    Interrupt_clearACKGroup(INTERRUPT_ACK_GROUP3);

    GPIO_writePin(CONTROL_DEBUG_GPIO, 0U);
}

//=============================================================================
// Funcoes internas
//=============================================================================

// Mede quanto tempo passou desde o inicio do periodo (contador em zero) ate
// aqui. Inclui a latencia de entrada na interrupcao.
static void control_measureIsrTime(void)
{
    uint16_t ticks = EPWM_getTimeBaseCounterValue(EPWM1_BASE);

    // Depois de 25 us o contador ja esta descendo: 2500 -> 0
    if(EPWM_getTimeBaseCounterDirection(EPWM1_BASE) ==
       EPWM_TIME_BASE_STATUS_COUNT_DOWN)
    {
        ticks = (2U * CONTROL_EPWM_TBPRD) - ticks;
    }

    control.isrTicks  = ticks;
    control.isrTimeUs = (float32_t)ticks / TBCLK_TICKS_PER_US;

    if(ticks > control.isrTicksMax)
    {
        control.isrTicksMax  = ticks;
        control.isrTimeMaxUs = control.isrTimeUs;
    }
}
