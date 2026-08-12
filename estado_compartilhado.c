#include "estado_compartilhado.h"
#include <string.h>
#include <stdio.h>

void estado_init(EstadoCompartilhado *estado) {
    // Zera o estado dos recursos
    for (int i = 0; i < NUM_RECURSOS; i++) {
        estado->recursos[i].ocupado = false;
        estado->recursos[i].titular[0] = '\0';
    }

    // Configura o Mutex como PROCESS_SHARED (interprocessos) para as Threads do servidor e o processo do monitor concorram pelo mesmo lock com segurança
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
    pthread_mutex_init(&estado->mutex, &attr);
    pthread_mutexattr_destroy(&attr);
}

int estado_reservar(EstadoCompartilhado *estado, int id, const char *titular) {
    if (id < 0 || id >= NUM_RECURSOS) return -1; // INVALID

    // fecha o mutex
    pthread_mutex_lock(&estado->mutex);

    // Encerra se estiver ocupado
    if (estado->recursos[id].ocupado) {
        pthread_mutex_unlock(&estado->mutex);
        return 1; // TAKEN
    }

    // ocupa, preenche o titular
    estado->recursos[id].ocupado = true; 
    strncpy(estado->recursos[id].titular, titular, TAM_TITULAR - 1);
    estado->recursos[id].titular[TAM_TITULAR - 1] = '\0';

    // libera o mutex
    pthread_mutex_unlock(&estado->mutex);
    return 0; // OK
}

int estado_cancelar(EstadoCompartilhado *estado, int id) {
    if (id < 0 || id >= NUM_RECURSOS) return -1; // INVALID

    pthread_mutex_lock(&estado->mutex);

    // Se já estava desocupado
    if (!estado->recursos[id].ocupado) {
        pthread_mutex_unlock(&estado->mutex);
        return 1; // FREE
    }

    // desocupa
    estado->recursos[id].ocupado = false;
    estado->recursos[id].titular[0] = '\0';

    pthread_mutex_unlock(&estado->mutex);
    return 0; // OK
}

int estado_status(EstadoCompartilhado *estado, int id, char *titular_out) {
    if (id < 0 || id >= NUM_RECURSOS) return -1; // INVALID

    pthread_mutex_lock(&estado->mutex);

    // Se está livre
    if (!estado->recursos[id].ocupado) {
        pthread_mutex_unlock(&estado->mutex);
        return 0; // FREE
    }

    // Informa o titular que está ocupando
    strncpy(titular_out, estado->recursos[id].titular, TAM_TITULAR);
    pthread_mutex_unlock(&estado->mutex);
    return 1; // TAKEN
}

void estado_list(EstadoCompartilhado *estado, char *mapa_out) {
    pthread_mutex_lock(&estado->mutex);

    // Informar através do mapa_out as vagas livres e ocupadas.
    for (int i = 0; i < NUM_RECURSOS; i++) {
        mapa_out[i] = estado->recursos[i].ocupado ? '1' : '0';
    }
    mapa_out[NUM_RECURSOS] = '\0';

    pthread_mutex_unlock(&estado->mutex);
}

// Snapshot consistente
void estado_snapshot(EstadoCompartilhado *estado) {
    pthread_mutex_lock(&estado->mutex); // Trava o mutex

    // Faz o snapshot registrando as condições dos recursos
    printf("=== SNAPSHOT DO ESTADO (INSPETOR) ===\n");
    for (int i = 0; i < NUM_RECURSOS; i++) {
        if (estado->recursos[i].ocupado) {
            printf("[%02d] TAKEN - Titular: %s\n", i, estado->recursos[i].titular); // ocupados
        } else {
            printf("[%02d] FREE\n", i); // e os liberados
        }
    }
    printf("======================================\n");

    // Libera o mutex pra voltar o funcionamento
    pthread_mutex_unlock(&estado->mutex);
}