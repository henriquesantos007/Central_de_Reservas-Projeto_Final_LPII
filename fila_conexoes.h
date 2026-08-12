#ifndef FILA_CONEXOES_H
#define FILA_CONEXOES_H

#include <pthread.h>

#define TAM_FILA 64 // Capacidade do buffer circular

typedef struct {
    int sockets[TAM_FILA];
    int inicio;
    int fim;
    int contagem;
    
    // Primitivas de sincronizacao interna do monitor da fila
    pthread_mutex_t mutex;
    pthread_cond_t nao_cheia;
    pthread_cond_t nao_vazia;
} FilaConexoes;

void fila_init(FilaConexoes *f);
void fila_enfileirar(FilaConexoes *f, int client_socket);
int fila_desenfileirar(FilaConexoes *f);
void fila_destruir(FilaConexoes *f);

#endif