//#############################################################################
//
// FILE:   encoder.h
//
// TITLE:  Leitura periodica do encoder Tamagawa (T-Format)
//
// Como o modulo funciona (uma leitura por periodo de controle):
//
//   ISR de controle (t = 0)       encoder_update()           usa a resposta
//                                 encoder_requestPosition()  pede a proxima
//   t = 0 .. 4,4 us               CLB + SPI enviam o comando ID0
//   t = 4,4 .. 7,6 us             encoder prepara a resposta
//   t = 7,6 .. 31,6 us            resposta chega (6 campos de 10 bits)
//   t ~ 32 us                     ISR do SPI guarda os bytes (dataReady)
//   t = 50 us                     proxima ISR de controle processa os bytes
//
// A CPU nao espera a comunicacao: o CLB e o SPI fazem a transacao sozinhos.
// O angulo usado em cada periodo foi medido pelo encoder ~46 us antes.
//
//#############################################################################

#ifndef ENCODER_H
#define ENCODER_H

#include "driverlib.h"

//-----------------------------------------------------------------------------
// Resultado da leitura. A variavel global "encoder" pode ser vista no Watch.
//-----------------------------------------------------------------------------
typedef struct
{
    // Leitura bruta
    uint32_t  position;          // posicao na volta, 0 .. 131071 contagens
    uint16_t  status;            // StatusField enviado pelo encoder

    // Grandezas calculadas a partir da posicao
    float32_t angleMech;         // angulo mecanico [rad], 0 .. 2*pi
    float32_t angleMechDeg;      // angulo mecanico [graus], para o Watch
    float32_t angleElec;         // angulo eletrico [rad], 0 .. 2*pi
    float32_t speedMech;         // velocidade mecanica filtrada [rad/s]
    float32_t speedRpm;          // velocidade mecanica filtrada [rpm]

    // Diagnostico
    bool      valid;             // a leitura deste periodo foi valida?
    bool      fault;             // erros consecutivos demais (travado)
    uint16_t  consecutiveErrors; // erros seguidos desde a ultima leitura boa
    uint32_t  readCount;         // total de leituras validas
    uint32_t  timeoutErrors;     // resposta nao chegou dentro do periodo
    uint32_t  idErrors;          // ControlField da resposta diferente do pedido
    uint32_t  crcErrors;         // CRC recebido diferente do calculado
} Encoder;

extern Encoder encoder;

//-----------------------------------------------------------------------------
// Funcoes
//-----------------------------------------------------------------------------

// Configura CLB, SPI, pinos e XBARs do T-Format e espera o encoder ligar.
// Chamar uma vez, antes de habilitar as interrupcoes.
void encoder_init(void);

// Inicia a leitura da posicao (comando ID0). Retorna na hora; a resposta
// chega ~32 us depois. Chamar uma vez por periodo de controle.
void encoder_requestPosition(void) __attribute__((ramfunc));

// Processa a resposta pedida no periodo anterior: confere erros e atualiza
// angulos e velocidade. Chamar antes de encoder_requestPosition().
void encoder_update(void) __attribute__((ramfunc));

#endif // ENCODER_H
