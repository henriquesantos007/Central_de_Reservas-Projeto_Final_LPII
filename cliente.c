#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> // Para resolver "localhost" ou IPs (Recomendação da IA)

int main(int argc, char *argv[]) {
    // Validação da invocação exigida: ./cliente <host> <porta>
    if (argc != 3) {
        fprintf(stderr, "Uso correto: %s <host> <porta>\n", argv[0]);
        exit(1);
    }

    const char *hostname = argv[1];
    int porta = atoi(argv[2]);

    // Criação do socket TCP
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("Erro ao criar socket");
        exit(1);
    }

    // Resolução do Host pra deixar usar "127.0.0.1" ou "localhost")
    struct hostent *server = gethostbyname(hostname);
    if (server == NULL) {
        fprintf(stderr, "Erro: Host '%s' nao encontrado.\n", hostname);
        exit(1);
    }

    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(porta);
    memcpy(&serv_addr.sin_addr.s_addr, server->h_addr, server->h_length);

    // Conecta ao servidor TCP
    if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Erro ao conectar no servidor");
        exit(1);
    }

    printf("Conectado a %s:%d! Digite os comandos (RESERVE, CANCEL, STATUS, LIST, QUIT):\n", hostname, porta);

    char buffer[1024];
    char resposta[1024];

    // Lê do teclado (stdin) até o usuário apertar Ctrl+D (EOF)
    while (fgets(buffer, sizeof(buffer), stdin) != NULL) {
        
        // Envia o comando digitado pro servidor
        if (write(sock, buffer, strlen(buffer)) < 0) {
            perror("Erro ao enviar dados");
            break;
        }

        // Fica bloqueado aguardando a resposta do servidor
        int bytes_lidos = read(sock, resposta, sizeof(resposta) - 1);
        if (bytes_lidos <= 0) {
            printf("\nConexão encerrada pelo servidor.\n");
            break;
        }

        resposta[bytes_lidos] = '\0';
        printf("%s", resposta); // A resposta já possui \n do servidor

        // Encerramos o cliente
        if (strcmp(resposta, "BYE\n") == 0) {
            break;
        }
    }

    close(sock);
    return 0;
}