#include "estado_compartilhado.h"
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#define SHM_NAME "/lpii_tp3_central"

int main() {
    // Abre a Memória Compartilhada existente sem O_CREAT
    int shm_fd = shm_open(SHM_NAME, O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("Erro ao abrir SHM. O servidor está rodando?");
        exit(1);
    }

    // Mapeia a memória para o inspetor
    EstadoCompartilhado *estado = mmap(NULL, sizeof(EstadoCompartilhado), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    
    // Tira a foto usando a API do Monitor encapsulada
    estado_snapshot(estado);

    // Desfaz o mapeamento local. Sendo que o inspetor nunca vai destruir o mutex e não faz shm_unlink.
    munmap(estado, sizeof(EstadoCompartilhado));
    close(shm_fd);

    return 0;
}