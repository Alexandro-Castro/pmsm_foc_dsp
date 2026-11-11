//#############################################################################
//
// FILE:   control.c
//
// TITLE:  Interrupcao de controle de 20 kHz
//
// Linha do tempo de um periodo (50 us):
//
//   t = 0       contador do ePWM1 em zero -> control_isr()
//                 1. pwm_checkTrip()           o hardware desligou o PWM?
//                 2. encoder_update()          usa a posicao pedida antes
//                 3. encoder_requestPosition() pede a proxima posicao
//                 4. PWM: comando de teste (etapa 2); depois, ADC -> FOC
//   t ~ 32 us   resposta do encoder pronta (ISR do SPI)
//   t = 50 us   proximo periodo; a razao ciclica escrita em (4) passa a valer
//
//#############################################################################

#include "control.h"
#include "encoder.h"
#include "pwm.h"
#include "app_config.h"
#include "device.h"

//-----------------------------------------------------------------------------
// Constantes
//-----------------------------------------------------------------------------
#define TBCLK_TICKS_PER_US      ((float32_t)(APP_EPWMCLK_HZ / 1000000UL))

//-----------------------------------------------------------------------------
// Variaveis do modulo
//-----------------------------------------------------------------------------
ControlStatus  control;                   // estado (visivel no Watch)
PwmTestCommand pwmTest;                   // comando de teste (Watch)

static uint16_t heartbeatTicks = 0U;      // contador para piscar o LED

//-----------------------------------------------------------------------------
// Funcoes internas
//-----------------------------------------------------------------------------
static void control_runPwmTest(void) __attribute__((ramfunc));
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

    // 3) Interrupcao no inicio de cada periodo do ePWM1 (contador em zero).
    //    A base de tempo do ePWM1 (20 kHz) e configurada em pwm_init().
    EPWM_setInterruptSource(EPWM1_BASE, EPWM_INT_TBCTR_ZERO);
    EPWM_setInterruptEventCount(EPWM1_BASE, 1U);
    EPWM_enableInterrupt(EPWM1_BASE);
    Interrupt_register(INT_EPWM1, &control_isr);

    control = (ControlStatus){0};

    pwmTest.enable  = false;
    pwmTest.duty[0] = 0.5f;
    pwmTest.duty[1] = 0.5f;
    pwmTest.duty[2] = 0.5f;
}

void control_start(void)
{
    EPWM_clearEventTriggerInterruptFlag(EPWM1_BASE);
    Interrupt_enable(INT_EPWM1);

    // Liga os contadores: a partir daqui a ISR roda a cada 50 us. As chaves
    // continuam desligadas ate pwm_enable().
    pwm_startTimeBase();
}

//=============================================================================
// Interrupcao de controle (20 kHz)
//=============================================================================
__interrupt void control_isr(void)
{
    GPIO_writePin(CONTROL_DEBUG_GPIO, 1U);

    // 1) Protecao: se o hardware desligou o PWM (trip zone), o comando de
    //    teste tambem e desligado. Religar exige uma acao nova no Watch.
    if(pwm_checkTrip())
    {
        pwmTest.enable = false;
    }

    // 2) Encoder: processa a resposta do pedido feito no periodo anterior
    encoder_update();

    // 3) Encoder: pede a posicao que sera usada no proximo periodo
    encoder_requestPosition();

    // 4) PWM: nesta etapa, razao ciclica fixa vinda do Watch. Nas proximas,
    //    leitura do ADC e FOC.
    control_runPwmTest();

    // 5) LED pisca a cada 0,5 s
    heartbeatTicks++;
    if(heartbeatTicks >= CONTROL_HEARTBEAT_TICKS)
    {
        heartbeatTicks = 0U;
        GPIO_togglePin(DEVICE_GPIO_PIN_LED1);
    }

    control.isrCount++;
    control_measureIsrTime();

    // 6) Libera a proxima interrupcao do ePWM1 (grupo 3 do PIE)
    EPWM_clearEventTriggerInterruptFlag(EPWM1_BASE);
    Interrupt_clearACKGroup(INTERRUPT_ACK_GROUP3);

    GPIO_writePin(CONTROL_DEBUG_GPIO, 0U);
}

//=============================================================================
// Funcoes internas
//=============================================================================

// Aplica o comando de teste escrito no Watch (pwmTest)
static void control_runPwmTest(void)
{
    if(pwmTest.enable && !pwm.enabled)
    {
        pwm_enable();                     // comeca em 50 %
    }
    else if(!pwmTest.enable && pwm.enabled)
    {
        pwm_disable();
    }

    if(pwm.enabled)
    {
        pwm_setDuty(pwmTest.duty[0], pwmTest.duty[1], pwmTest.duty[2]);
    }
}

// Mede quanto tempo passou desde o inicio do periodo (contador em zero) ate
// aqui. Inclui a latencia de entrada na interrupcao.
static void control_measureIsrTime(void)
{
    uint16_t ticks = EPWM_getTimeBaseCounterValue(EPWM1_BASE);

    // Depois de 25 us o contador ja esta descendo: 2500 -> 0
    if(EPWM_getTimeBaseCounterDirection(EPWM1_BASE) ==
       EPWM_TIME_BASE_STATUS_COUNT_DOWN)
    {
        ticks = (2U * PWM_TBPRD) - ticks;
    }

    control.isrTicks  = ticks;
    control.isrTimeUs = (float32_t)ticks / TBCLK_TICKS_PER_US;

    if(ticks > control.isrTicksMax)
    {
        control.isrTicksMax  = ticks;
        control.isrTimeMaxUs = control.isrTimeUs;
    }
}
