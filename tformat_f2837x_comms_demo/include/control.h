//#############################################################################
//
// FILE:   control.h
//
// TITLE:  Interrupcao de controle de 20 kHz
//
// O ePWM1 gera uma interrupcao a cada 50 us, no inicio de cada periodo de
// chaveamento (contador em zero). Nesta etapa ela so le o encoder; nas
// proximas, recebera a leitura do ADC, o FOC e a atualizacao do PWM.
//
// O ePWM1 ainda nao aciona nenhum pino: so a base de tempo e usada.
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

// Configura o ePWM1 (parado) e os pinos de diagnostico. Chamar uma vez.
void control_init(void);

// Liga o contador do ePWM1 e a interrupcao. Chamar depois de inicializar
// todos os modulos usados pela interrupcao (por exemplo, o encoder).
void control_start(void);

#endif // CONTROL_H
