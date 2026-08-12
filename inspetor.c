#include "estado_compartilhado.h"
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <string.h>

int main(int argc, char *argv[]) {
    char shm_name[256] = "/lpii_tp3";

    // le nome da shm se passado no argv
    if (argc >= 2) {
        strncpy(shm_name, argv[1], sizeof(shm_name) - 1);
        shm_name[sizeof(shm_name) - 1] = '\0';
    }

    // Abre a Memória Compartilhada existente sem só pra leitura e escrita
    int shm_fd = shm_open(shm_name, O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("Erro ao abrir SHM. O servidor está rodando?");
        exit(1);
    }

    // Mapeia a memória para o inspetor
    EstadoCompartilhado *estado = mmap(NULL, sizeof(EstadoCompartilhado), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (estado == MAP_FAILED) {
        perror("Erro no mmap");
        exit(1);
    }
    
    // Tira a foto consistente
    estado_snapshot(estado);

    // Desfaz o mapeamento local. Sendo que o inspetor nunca vai destruir o mutex e não faz shm_unlink.
    munmap(estado, sizeof(EstadoCompartilhado));
    close(shm_fd);

    return 0;
}