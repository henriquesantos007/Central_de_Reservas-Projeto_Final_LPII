#include "../estado_compartilhado.h"
#include <stdio.h>
#include <assert.h>

int main() {
    EstadoCompartilhado estado;
    estado_init(&estado);

    char titular[TAM_TITULAR];
    char mapa[NUM_RECURSOS + 1];

    // Primeiro teste: Reservar recurso 0
    assert(estado_reservar(&estado, 0, "Alice") == 0);
    assert(estado_reservar(&estado, 0, "Bob") == 1); // TAKEN

    // Segundo teste: Status do recurso 0 e limite de borda
    assert(estado_status(&estado, 0, titular) == 1); // TAKEN
    assert(estado_status(&estado, 99, titular) == -1); // INVALID

    // Terceiro teste: Cancelamento
    assert(estado_cancelar(&estado, 0) == 0); // OK
    assert(estado_cancelar(&estado, 0) == 1); // FREE

    // Quarto teste: Verificação do Mapa
    estado_reservar(&estado, 5, "Carlos");
    estado_list(&estado, mapa);
    assert(mapa[5] == '1');
    assert(mapa[0] == '0');

    printf("Tudo certo aqui!\n");
    return 0;
}