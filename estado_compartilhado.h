#ifndef ESTADO_COMPARTILHADO_H
#define ESTADO_COMPARTILHADO_H

#include <pthread.h>
#include <stdbool.h>

// Exigências do Caminho B
#define NUM_RECURSOS 64
#define TAM_TITULAR 32 

// Estrutura de uma reserva individual
typedef struct {
    bool ocupado; // 0 = livre e 1 = ocupado
    char titular[TAM_TITULAR];
} Recurso;


// Estrutura que ficara na memória compartilhada
typedef struct {
    Recurso recursos[NUM_RECURSOS];
    
    // Primitiva interprocessos: Mutex com atributo PROCESS_SHARED
    pthread_mutex_t mutex;
} EstadoCompartilhado;



// API do motor pra manipular o mapa de reservas. Recebendo somente os dados de domínio pra que nenhuma função externa toque no mutex diretamente 

// Inicializa a estrutura e configura o mutex como interprocessos
void estado_init(EstadoCompartilhado *estado);

// Tenta reservar. Retorna: 0 pra OK, 1 pra TAKEN e -1 pra INVALID
int estado_reservar(EstadoCompartilhado *estado, int id, const char *titular);

// Tenta cancelar. Retorna: 0 pra OK, 1 pra FREE e -1 para INVALID
int estado_cancelar(EstadoCompartilhado *estado, int id);

// Consulta o status. Retorna: 0 para FREE, 1 para TAKEN (copia o nome para titular_out pra saber quem ocupa) e -1 pra INVALID
int estado_status(EstadoCompartilhado *estado, int id, char *titular_out);

// Gera a sequência de 64 caracteres (0 ou 1) para o comando LIST
void estado_list(EstadoCompartilhado *estado, char *mapa_out);

// Snapshot consistente pra uso do processo Inspetor
void estado_snapshot(EstadoCompartilhado *estado);

#endif