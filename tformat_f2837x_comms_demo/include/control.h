//#############################################################################
//
// FILE:   control.h
//
// TITLE:  Interrupcao de controle de 20 kHz
//
// O ePWM1 (fase A do inversor) gera uma interrupcao a cada 50 us, no inicio
// de cada periodo de chaveamento (contador em zero). Nela rodam a leitura do
// encoder e o comando do PWM; nas proximas etapas, a leitura do ADC e o FOC.
//
//#############################################################################

#ifndef CONTROL_H
#define CONTROL_H

#include "driverlib.h"

//-----------------------------------------------------------------------------
// Estado da interrupcao de controle. A variavel global "control" pode ser
// vista no Watch.
//-----------------------------------------------------------------------------
typedef struct
{
    uint32_t  isrCount;          // numero de vezes que a ISR executou
    uint16_t  isrTicks;          // duracao da ultima ISR [ciclos de 10 ns]
    uint16_t  isrTicksMax;       // maior duracao observada [ciclos de 10 ns]
    float32_t isrTimeUs;         // duracao da ultima ISR [us]
    float32_t isrTimeMaxUs;      // maior duracao observada [us]
} ControlStatus;

extern ControlStatus control;

//-----------------------------------------------------------------------------
// Teste do PWM na bancada (etapa 2). Os campos sao escritos pelo Watch do CCS.
//
//   pwmTest.enable = 1   libera o chaveamento (comeca em 50 %)
//   pwmTest.duty[i]      razao ciclica de cada fase, de 0 a 1
//   pwmTest.enable = 0   desliga todas as chaves
//
// Se o PWM for desligado pelo hardware (por exemplo, ao pausar no CCS),
// pwmTest.enable volta para 0 sozinho: para religar, escreva 1 de novo.
//-----------------------------------------------------------------------------
typedef struct
{
    bool      enable;            // 1 = chavear, 0 = desligar
    float32_t duty[3];           // razao ciclica das fases A, B, C
} PwmTestCommand;

extern PwmTestCommand pwmTest;

// Configura a interrupcao do ePWM1 e os pinos de diagnostico. Chamar depois
// de pwm_init(). Chamar uma vez.
void control_init(void);

// Liga os contadores do PWM e a interrupcao. Chamar depois de inicializar
// todos os modulos usados pela interrupcao (por exemplo, o encoder).
void control_start(void);

#endif // CONTROL_H
