#include "estado_compartilhado.h"
#include "fila_conexoes.h"
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define NUM_WORKERS 4 // 4 threads fixas


// globais pra limpeza no encerramento
char shm_name_global[256] = "/lpii_tp3";
EstadoCompartilhado *estado_global = NULL;
FilaConexoes fila_global;
int shm_fd_global = -1;
int server_fd_global = -1;
volatile sig_atomic_t rodando = 1;

// handler seguro pros sinais
void tratar_sinal(int sig) {
    (void)sig;
    rodando = 0;
    if (server_fd_global != -1) {
        close(server_fd_global);
    }
}

// Thread worker pra cada cliente
void tratar_cliente(int client_socket) {
    char buffer[1024];
    char resposta[1024];
    ssize_t bytes_lidos;

    // Fica em loop lendo os comandos do cliente até ele se desconectar
    while ((bytes_lidos = read(client_socket, buffer, sizeof(buffer) - 1)) > 0) {
        buffer[bytes_lidos] = '\0';
        
        // Remove a quebra de linha do final do comando
        buffer[strcspn(buffer, "\r\n")] = 0; 
        
        char comando[32], titular[TAM_TITULAR];
        int id = -1;

        // Tenta extrair COMANDO, ID e TITULAR da string enviada
        int itens = sscanf(buffer, "%31s %d %31s", comando, &id, titular);


        // Roteamento do Protocolo Canônico
        if (strcmp(comando, "LIST") == 0) {
            char mapa[NUM_RECURSOS + 1];
            estado_list(estado_global, mapa);
            snprintf(resposta, sizeof(resposta), "MAP %s\n", mapa);
        
        } else if (strcmp(comando, "RESERVE") == 0 && itens >= 3) {
            int res = estado_reservar(estado_global, id, titular);
            if (res == 0) strcpy(resposta, "OK\n");
            else if (res == 1) strcpy(resposta, "TAKEN\n");
            else strcpy(resposta, "INVALID\n");

        } else if (strcmp(comando, "CANCEL") == 0 && itens >= 2) {
            int res = estado_cancelar(estado_global, id);
            if (res == 0) strcpy(resposta, "OK\n");
            else if (res == 1) strcpy(resposta, "FREE\n");
            else strcpy(resposta, "INVALID\n");

        } else if (strcmp(comando, "STATUS") == 0 && itens >= 2) {
            char titular_out[TAM_TITULAR];
            int res = estado_status(estado_global, id, titular_out);
            if (res == 0) strcpy(resposta, "FREE\n");
            else if (res == 1) snprintf(resposta, sizeof(resposta), "TAKEN %s\n", titular_out);
            else strcpy(resposta, "INVALID\n");

        } else if (strcmp(comando, "QUIT") == 0) {
            strcpy(resposta, "BYE\n");
            write(client_socket, resposta, strlen(resposta));
            break; // Sai do loop e encerra a thread
            
        } else {
            strcpy(resposta, "ERR Comando desconhecido ou malformado\n");
        }

        // Envia a resposta de volta ao cliente
        write(client_socket, resposta, strlen(resposta));
    }

    close(client_socket);
}

void *worker_loop(void *arg) {
    (void)arg;
    while (1) {
        int client_socket = fila_desenfileirar(&fila_global);
        if (client_socket == -1) break; // sinal de encerramento
        tratar_cliente(client_socket);
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    // Validação da invocação obrigatoria
    if (argc < 2) {
        fprintf(stderr, "Uso: %s <porta> [nome shm]\n", argv[0]);
        exit(1);
    }

    int porta = atoi(argv[1]);
    
    // Sobrescreve nome padrão da shm se passado no argv
    if (argc >= 3) {
        strncpy(shm_name_global, argv[2], sizeof(shm_name_global) - 1);
        shm_name_global[sizeof(shm_name_global) - 1] = '\0';
    }

    // Registra o tratamento de sinais
    signal(SIGINT, tratar_sinal);
    signal(SIGTERM, tratar_sinal);
    signal(SIGPIPE, SIG_IGN);

    // Cria a Memória Compartilhada
    shm_fd_global = shm_open(shm_name_global, O_CREAT | O_RDWR, 0666);
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

    // Setup do Thread Pool
    fila_init(&fila_global);

    // Preparando as threads antes de começar
    pthread_t workers[NUM_WORKERS];
    for (int i = 0; i < NUM_WORKERS; i++) {
        pthread_create(&workers[i], NULL, worker_loop, NULL);
    }

    // Setup do Socket TCP
    server_fd_global = socket(AF_INET, SOCK_STREAM, 0);

    // Evita o erro "Address already in use" ao reiniciar o servidor rapidamente
    int opt = 1;
    setsockopt(server_fd_global, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(porta);

    bind(server_fd_global, (struct sockaddr*)&address, sizeof(address));
    listen(server_fd_global, 10); // backlog

    printf("SHM '%s' alocada. Servidor escutando na porta %d\n", shm_name_global, porta);

    // Loop de aceitação
    while (rodando) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        
        // Fica bloqueado aqui até um cliente conectar
        int client_socket = accept(server_fd_global, (struct sockaddr*)&client_addr, &client_len);
        if (client_socket < 0) continue;

        // O produtor apenas enfileira e volta a esperar novas conexoes
        fila_enfileirar(&fila_global, client_socket);
    }

    // cleanup principal
    printf("\nLimpando recursos e encerrando...\n");

    fila_encerrar(&fila_global);
    for (int i = 0; i < NUM_WORKERS; i++) {
        pthread_join(workers[i], NULL);
    }

    if (estado_global != NULL) {
        pthread_mutex_destroy(&estado_global->mutex);
        munmap(estado_global, sizeof(EstadoCompartilhado));
    }
    if (shm_fd_global != -1) {
        shm_unlink(shm_name_global);
    }
    fila_destruir(&fila_global);

    return 0;
}