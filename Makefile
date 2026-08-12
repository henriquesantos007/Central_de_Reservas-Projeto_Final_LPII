# Compilador e flags de compilação exigidas (C17, warnings e suporte a threads)
CC = gcc
CFLAGS = -Wall -Wextra -pthread -std=c17
LDFLAGS = -lrt

# Alvo padrão exigido pelo corretor automatizado
all: servidor cliente inspetor

# Receita para compilar o servidor
servidor: servidor.c estado_compartilhado.c estado_compartilhado.h fila_conexoes.c fila_conexoes.h
	$(CC) $(CFLAGS) servidor.c estado_compartilhado.c fila_conexoes.c -o servidor $(LDFLAGS)

# Receita para compilar o cliente (não precisa da biblioteca de tempo real -lrt, mas usa as mesmas CFLAGS)
cliente: cliente.c
	$(CC) $(CFLAGS) cliente.c -o cliente

# Receita para compilar o inspetor
inspetor: inspetor.c estado_compartilhado.c estado_compartilhado.h
	$(CC) $(CFLAGS) inspetor.c estado_compartilhado.c -o inspetor $(LDFLAGS)

# Alvo utilitário para limpar a sujeira (apagar os executáveis)
clean:
	rm -f servidor cliente inspetor