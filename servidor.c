#include "estado_compartilhado.h"
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <signal.h>

// Apenas alocará a memória, vai tratar o sinal de interrupção Ctrl+C para limpar a memória direitinho e ficará dormindo.

// Nome padrão do segmento de memória
#define SHM_NAME "/lpii_tp3_central"

// Variáveis globais apenas para o handler de sinal conseguir limpar os recursos
EstadoCompartilhado *estado_global = NULL;
int shm_fd_global = -1;

// Handler para limpar a memória quando a gente apertar Ctrl+C
void tratar_sinal(int sig) {
    (void)sig; // Suprime aviso de variável não usada
    printf("\n[Servidor] Encerrando... Limpando memória compartilhada.\n");
    
    if (estado_global != NULL) {
        pthread_mutex_destroy(&estado_global->mutex);
        munmap(estado_global, sizeof(EstadoCompartilhado));
    }
    if (shm_fd_global != -1) {
        close(shm_fd_global);
        shm_unlink(SHM_NAME); // Remove o segmento órfão
    }
    exit(0);
}

int main() {
    // Registra o tratamento de sinais
    signal(SIGINT, tratar_sinal);
    signal(SIGTERM, tratar_sinal);

    // Cria a Memória Compartilhada
    shm_fd_global = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (shm_fd_global == -1) {
        perror("Erro no shm_open");
        exit(1);
    }

    // Dimensiona o tamanho exato da estrutura de tamanho fixo
    ftruncate(shm_fd_global, sizeof(EstadoCompartilhado));

    // Mapeia para o espaço de endereçamento deste processo
    estado_global = mmap(NULL, sizeof(EstadoCompartilhado), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd_global, 0);
    
    // Inicializa o estado (zera recursos e cria o Mutex interprocessos)
    estado_init(estado_global);

    printf("[Servidor] Memória Compartilhada '%s' alocada com sucesso!\n", SHM_NAME);
    printf("[Servidor] Servidor inativo aguardando conexões (Pressione Ctrl+C para encerrar)...\n");

    // Loop infinito simulando a vida do servidor
    while (1) {
        pause(); 
    }

    return 0;
}