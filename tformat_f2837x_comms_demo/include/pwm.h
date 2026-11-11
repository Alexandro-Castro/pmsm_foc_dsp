//#############################################################################
//
// FILE:   pwm.h
//
// TITLE:  PWM trifasico do inversor (ePWM1, ePWM2, ePWM3)
//
// Ligacao com a placa inversora (conector JP1, modulo IRAMX16UP60A):
//
//   Fase  ePWM   Chave superior                Chave inferior
//   A     ePWM1  GPIO0 J4-40 -> PWM0 -> HIN1    GPIO1 J4-39 -> PWM1 -> LIN1
//   B     ePWM2  GPIO2 J4-38 -> PWM2 -> HIN2    GPIO3 J4-37 -> PWM3 -> LIN2
//   C     ePWM3  GPIO4 J4-36 -> PWM4 -> HIN3    GPIO5 J4-35 -> PWM5 -> LIN3
//
//   Ativa: GPIO24 J4-34 -> JP1-4      Falha: JP1-2 -> GPIO16 J4-33
//
// LOGICA: a chave LIGA com o pino do DSP em 1.
//   pino em 1 -> LED do optoacoplador aceso -> saida do opto em 0 ->
//   entrada HIN/LIN do IRAM (ativa em 0) acionada -> chave ligada.
// Pino em 0 ou solto (DSP em reset, sem alimentacao) -> chave desligada.
// No tempo morto e no estado seguro, os 6 pinos ficam em 0.
//
// Razao ciclica (duty): fracao do periodo em que a chave SUPERIOR fica ligada,
// de 0 a 1. A chave inferior e o complemento, descontado o tempo morto.
//
// Protecao: as 6 saidas sao forcadas para 0 e "Ativa" vai para 0
//   - ao iniciar (o PWM so chaveia depois de pwm_enable());
//   - por pwm_disable();
//   - se a placa sinalizar falha (pino Falha em 1): TZ2;
//   - se o relogio do sistema falhar: TZ5;
//   - quando a CPU para no depurador (pause/breakpoint no CCS): TZ6.
// Depois de desligar, o PWM so volta com uma nova chamada a pwm_enable().
//
//#############################################################################

#ifndef PWM_H
#define PWM_H

#include "driverlib.h"

// Causas registradas em pwm.tripSource (bits do TZOSTFLG do ePWM1)
#define PWM_TRIP_BOARD_FAULT    0x02U   // TZ2: pino Falha da placa em 1
#define PWM_TRIP_CLOCK_FAIL     0x10U   // TZ5: falha do relogio
#define PWM_TRIP_CPU_HALT       0x20U   // TZ6: CPU parada pelo depurador

//-----------------------------------------------------------------------------
// Estado do PWM. A variavel global "pwm" pode ser vista no Watch.
//-----------------------------------------------------------------------------
typedef struct
{
    bool      enabled;       // true = chaveando; false = tudo desligado
    float32_t duty[3];       // razao ciclica aplicada nas fases A, B, C
    uint16_t  cmpa[3];       // valor correspondente no comparador (CMPA)
    uint32_t  tripCount;     // quantas vezes o hardware desligou o PWM
    uint16_t  tripSource;    // causa do ultimo desligamento (PWM_TRIP_...)
} PwmStatus;

extern PwmStatus pwm;

// Configura ePWM1-3 e os pinos com todas as chaves DESLIGADAS. Chamar logo
// no inicio do programa, antes dos demais modulos.
void pwm_init(void);

// Liga os contadores (base de tempo de 20 kHz). As saidas continuam
// desligadas ate pwm_enable().
void pwm_startTimeBase(void);

// Habilita o modulo IRAM (Ativa = 1) e libera o chaveamento, comecando com
// 50 % nas tres fases. O IRAM so passa a chavear ~8 ms depois de Ativa
// subir (tempo de saida do estado de falha do T/ITRIP).
void pwm_enable(void) __attribute__((ramfunc));

// Desliga todas as chaves imediatamente e desabilita o modulo (Ativa = 0).
void pwm_disable(void) __attribute__((ramfunc));

// Define a razao ciclica das tres fases (limitada entre 0 e 1). O valor novo
// vale a partir do proximo inicio de periodo (contador em zero).
void pwm_setDuty(float32_t dutyA, float32_t dutyB, float32_t dutyC)
    __attribute__((ramfunc));

// Verifica se o hardware desligou o PWM (trip zone). Retorna true na
// interrupcao em que o desligamento e detectado. Chamar a cada periodo.
bool pwm_checkTrip(void) __attribute__((ramfunc));

#endif // PWM_H
