//#############################################################################
//
// FILE:   app_config.h
//
// TITLE:  Parametros da aplicacao (PMSM 0,75 kW + LAUNCHXL-F28379D)
//
// Todas as constantes ajustaveis ficam aqui, para nao ficarem espalhadas
// pelo codigo. Os demais modulos so leem estes valores.
//
//#############################################################################

#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#include "device.h"

//-----------------------------------------------------------------------------
// Relogios
//-----------------------------------------------------------------------------
// EPWMCLK = SYSCLK / 2 = 100 MHz. Alimenta os modulos ePWM e tambem o CLB que
// gera a temporizacao do T-Format (veja encoder.c). Mudar este valor quebra a
// comunicacao com o encoder.
#define APP_EPWMCLK_HZ              (DEVICE_SYSCLK_FREQ / 2U)

//-----------------------------------------------------------------------------
// Malha de controle
//-----------------------------------------------------------------------------
// A interrupcao de controle roda na mesma frequencia de chaveamento do
// inversor: 20 kHz -> um periodo de 50 us.
#define CONTROL_FREQ_HZ             20000U
#define CONTROL_PERIOD_S            (1.0f / (float32_t)CONTROL_FREQ_HZ)

// O ePWM1 conta para cima e para baixo (triangular, como no PWM simetrico do
// FOC). Um periodo completo = 2 * TBPRD ciclos de TBCLK.
//   TBPRD = 100 MHz / (2 * 20 kHz) = 2500
#define CONTROL_EPWM_TBPRD          (APP_EPWMCLK_HZ / (2U * CONTROL_FREQ_HZ))

// Pisca o LED azul (LD10) a cada 0,5 s para mostrar que a interrupcao roda.
#define CONTROL_HEARTBEAT_TICKS     (CONTROL_FREQ_HZ / 2U)

// Pino de diagnostico: fica em 1 enquanto a interrupcao de controle executa.
// GPIO8 = J8-78 (mesmo conector do encoder). Use no osciloscopio para ver a
// duracao da interrupcao e a relacao com o TxEN (J8-77).
#define CONTROL_DEBUG_GPIO          8U
#define CONTROL_DEBUG_GPIO_CFG      GPIO_8_GPIO8

//-----------------------------------------------------------------------------
// Encoder Tamagawa TS5669N1138 (T-Format)
//-----------------------------------------------------------------------------
// 17 bits por volta, confirmado em bancada (posicao vai de 0 a 131071).
#define ENCODER_BITS                17U
#define ENCODER_COUNTS_PER_REV      (1UL << ENCODER_BITS)       // 131072
#define ENCODER_COUNTS_MASK         (ENCODER_COUNTS_PER_REV - 1UL)

// Tempo para o encoder iniciar depois de ligado (valor do exemplo da TI).
#define ENCODER_POWER_ON_DELAY_US   1500000UL

// Filtro passa-baixas de 1a ordem da velocidade. Com amostragem a cada 50 us,
// uma contagem do encoder equivale a ~9,2 rpm; sem filtro a velocidade fica
// muito ruidosa. Constante de tempo de 1 ms (frequencia de corte ~160 Hz).
#define ENCODER_SPEED_FILTER_TAU_S  0.001f

// Quantas leituras seguidas com erro ativam encoder.fault. A falha fica
// travada ate reiniciar o programa (nas proximas etapas ela vai desligar o
// PWM do inversor).
#define ENCODER_MAX_CONSECUTIVE_ERRORS  3U

//-----------------------------------------------------------------------------
// Motor (AJUSTAR)
//-----------------------------------------------------------------------------
// Numero de pares de polos. Valor provisorio: o angulo eletrico so fica
// correto depois de colocar o valor real do motor de 0,75 kW.
#define MOTOR_POLE_PAIRS            1U

// Deslocamento entre o zero do encoder e o eixo d do rotor, em contagens
// eletricas (0 .. 131071). Sera medido na etapa 4 (alinhamento do rotor).
#define ENCODER_ELEC_OFFSET_COUNTS  0UL

#endif // APP_CONFIG_H
