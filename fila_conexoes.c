#include "fila_conexoes.h"

void fila_init(FilaConexoes *f) {
    f->inicio = 0;
    f->fim = 0;
    f->contagem = 0;
    f->encerrando = 0;
    pthread_mutex_init(&f->mutex, NULL);
    pthread_cond_init(&f->nao_cheia, NULL);
    pthread_cond_init(&f->nao_vazia, NULL);
}

void fila_enfileirar(FilaConexoes *f, int client_socket) {
    pthread_mutex_lock(&f->mutex);
    
    // Espera se a fila estiver lotada
    while (f->contagem == TAM_FILA) {
        pthread_cond_wait(&f->nao_cheia, &f->mutex);
    }
    
    f->sockets[f->fim] = client_socket;
    f->fim = (f->fim + 1) % TAM_FILA;
    f->contagem++;
    
    // Avisa um consumidores ocioso que chegou trabalho
    pthread_cond_signal(&f->nao_vazia);
    
    pthread_mutex_unlock(&f->mutex);
}

int fila_desenfileirar(FilaConexoes *f) {
    pthread_mutex_lock(&f->mutex);
    
    // CONSUMIDOR: Dorme se nao houver clientes na fila E o servidor nao estiver encerrando
    while (f->contagem == 0 && !f->encerrando) {
        pthread_cond_wait(&f->nao_vazia, &f->mutex);
    }

    // Fila vazia e encerramento sinalizado: acorda o worker sem trabalho, ele deve sair
    if (f->contagem == 0 && f->encerrando) {
        pthread_mutex_unlock(&f->mutex);
        return -1; // sentinela de encerramento
    }
    
    int client_socket = f->sockets[f->inicio];
    f->inicio = (f->inicio + 1) % TAM_FILA;
    f->contagem--;
    
    // Avisa ao produtor que liberou um espaco na fila
    pthread_cond_signal(&f->nao_cheia);
    
    pthread_mutex_unlock(&f->mutex);
    
    return client_socket;
}

void fila_encerrar(FilaConexoes *f) {
    pthread_mutex_lock(&f->mutex);
    f->encerrando = 1;
    // Acorda TODOS os consumidores ociosos (broadcast, não signal) para que cada um reavalie a condicao de parada e saia do loop de espera
    pthread_cond_broadcast(&f->nao_vazia);
    pthread_mutex_unlock(&f->mutex);
}

void fila_destruir(FilaConexoes *f) {
    pthread_mutex_destroy(&f->mutex);
    pthread_cond_destroy(&f->nao_cheia);
    pthread_cond_destroy(&f->nao_vazia);
}