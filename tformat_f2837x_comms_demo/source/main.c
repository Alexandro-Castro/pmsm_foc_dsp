//#############################################################################
//
// FILE:   main.c
//
// TITLE:  Acionamento do PMSM de 0,75 kW - LAUNCHXL-F28379D
//
// Etapa 1: leitura periodica do encoder Tamagawa (T-Format) a 20 kHz.
// Etapa 2: PWM trifasico do inversor (20 kHz, tempo morto de 1 us).
//
// Estrutura do codigo:
//   app_config.h   parametros ajustaveis (frequencias, encoder, motor)
//   pwm.c/.h       PWM do inversor (ePWM1-3) e protecao (trip zone)
//   control.c/.h   interrupcao de controle de 20 kHz (ePWM1)
//   encoder.c/.h   leitura do encoder, angulos e velocidade
//   main.c         inicializacao; depois disso tudo acontece nas
//                  interrupcoes
//
// Variaveis para o Watch: encoder, control, pwm, pwmTest
//
//#############################################################################

#include "driverlib.h"
#include "device.h"
#include "pwm.h"
#include "control.h"
#include "encoder.h"

void main(void)
{
    // 1) Chip: relogios (SYSCLK = 200 MHz), memoria flash e perifericos
    Device_init();
    Device_initGPIO();

    // EPWMCLK = SYSCLK / 2 = 100 MHz. E o valor padrao, mas fica explicito
    // porque o PWM e o CLB do encoder dependem dele (veja app_config.h).
    SysCtl_setEPWMClockDivider(SYSCTL_EPWMCLK_DIV_2);

    // 2) PWM primeiro: coloca as 6 saidas do inversor no estado seguro
    //    (todas as chaves desligadas) o quanto antes.
    pwm_init();

    // 3) Tabela de interrupcoes (PIE) limpa e interrupcoes desligadas
    Interrupt_initModule();
    Interrupt_initVectorTable();

    // 4) Modulos da aplicacao
    control_init();     // interrupcao do ePWM1, LED e pino de diagnostico
    encoder_init();     // T-Format; espera ~1,5 s o encoder iniciar

    // 5) Liga os contadores do PWM e a interrupcao de controle. As chaves
    //    continuam desligadas ate pwmTest.enable = 1 (Watch).
    control_start();
    EINT;               // habilita interrupcoes (INTM)
    ERTM;               // habilita depuracao em tempo real (DBGM)

    // 6) Laco de fundo: livre. O trabalho periodico roda em control_isr().
    for(;;)
    {
    }
}
