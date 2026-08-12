# Trabalho Prático 3 - Comunicação (Módulo 3)
**Disciplina:** Linguagem de Programação II (LPII)

## 1. Cenário Escolhido
**Cenário B: Central de Reservas**.
O sistema gerencia um conjunto fixo de N=64 recursos (como assentos ou vagas) indexados de 0 a 63. Os clientes efetuam reservas e cancelamentos concorrentemente, e o desafio central resolvido pelo sistema é impedir a dupla reserva do mesmo recurso sob concorrência.

## 2. Protocolo Implementado
O servidor TCP atende requisições baseadas no seguinte protocolo canônico, com uma requisição por linha e campos separados por espaço:
*   `LIST`: Retorna `MAP <sequência de 64 caracteres 0/1>` indicando o status atual de todos os recursos (0 = livre, 1 = ocupado).
*   `RESERVE <id> <titular>`: Retorna `OK` se reservou com sucesso, `TAKEN` se já estava ocupado, ou `INVALID` se o identificador estiver fora da faixa permitida.
*   `CANCEL <id>`: Retorna `OK` se liberou com sucesso, `FREE` se o recurso já constava como livre, ou `INVALID`.
*   `STATUS <id>`: Retorna `FREE` se livre, `TAKEN <titular>` se estiver ocupado, ou `INVALID`.
*   `QUIT`: Retorna `BYE` e o servidor fecha o socket da conexão.
*   Qualquer outro comando malformado retorna `ERR <motivo>`.

## 3. Justificativa da Primitiva de Sincronização
O cenário modela um sistema com forte concorrência, focado em operações de checagem condicional atômica. Ao receber um comando `RESERVE`, o sistema executa um fluxo de *test-and-set* (verificar e ocupar sem janela de interrupção).

**Por que não usar rwlock (Leitores-Escritores)?**
Embora o sistema possua comandos de leitura pura (como `STATUS` e `LIST`), a principal transação crítica (`RESERVE`) exige que a *thread* faça uma verificação de leitura e, em seguida, uma modificação de escrita imediata se o recurso estiver livre. Como o *rwlock* não suporta a promoção atômica (*upgrade*) de um *lock* de leitura para um de escrita, teríamos que iniciar o `RESERVE` diretamente com um *lock* exclusivo de escrita, minando os benefícios do *rwlock*.

**A Escolha: Mutex (`pthread_mutex_t`)**
A primitiva de sincronização escolhida foi o Mutex com o atributo `PTHREAD_PROCESS_SHARED`, que reside fisicamente dentro do segmento de Memória Compartilhada (SHM) POSIX. Ele resolve com perfeição o requisito de exclusão mútua total. Como a verificação e alteração dos dados na memória principal (uma checagem condicional simples seguida de uma cópia de *string*) ocorrem na escala de nanossegundos, o custo de aquisição do *lock* do Mutex é baixíssimo. O comportamento bloqueante do Mutex no kernel gerencia de forma justa a disputa das *threads*, eliminando com máxima eficiência o risco de perda de atualização ou dupla reserva entre as requisições.

## 4. Instruções de Build e Execução
Este projeto fornece um `Makefile` que automatiza a geração dos binários exigidos com a padronização adequada (Padrão C17).

**Compilação:**
Para gerar os binários (`servidor`, `cliente`, `inspetor`), execute na raiz do projeto:
```bash
make
```

**Execução:**
1. Inicie o servidor (dono do segmento SHM e roteador das conexões TCP):
   ```bash
   ./servidor <porta> [nome_shm]
   # Exemplo: ./servidor 8080 /minhacentral
   ```
2. Inicie o processo cliente de forma interativa:
   ```bash
   ./cliente <host> <porta>
   # Exemplo: ./cliente 127.0.0.1 8080
   ```
3. Opcionalmente, ateste a comunicação interprocessos nativa sem rede utilizando o inspetor:
   ```bash
   ./inspetor [nome_shm]
   # Exemplo: ./inspetor /minhacentral
   ```

**Limpeza:**
Para apagar os executáveis do diretório de trabalho:
```bash
make clean
```

## 5. Declaração de Uso de Inteligência Artificial
Durante o ciclo de desenvolvimento deste trabalho prático, fiz uso de uma ferramenta de Inteligência Artificial (LLM) como assistente de codificação e tutoria de *debugging*. O apoio da IA ocorreu nas seguintes situações:
*   **Race Condition no Socket:** Inicialmente, meu servidor estava passando o endereço de uma variável de escopo local para o `pthread_create` das *threads* de atendimento. A IA me auxiliou apontando o erro e sugerindo que eu precisava obrigatoriamente alocar o socket de forma dinâmica no *heap* (`malloc(sizeof(int))`) para evitar o sobrescrevimento do valor (uma clássica condição de corrida) quando múltiplos clientes conectavam muito rapidamente.
*   **Detecção de Warnings Implícitas:** Usei a IA para decifrar alertas do compilador `gcc` (com as *flags* `-Wall -Wextra`). Fui instruído de que o erro *incompatible implicit declaration of built-in function* ocorria por esquecer a inclusão da biblioteca `<string.h>` necessária para operações básicas de texto como `strcmp` e `strcpy`.
*   **Diagnóstico da Conexão TCP (Comando QUIT):** Ao testar o encerramento do protocolo TCP pelo utilitário `netcat`, notei que o terminal não liberava após a mensagem `BYE`. A IA serviu como tutor para explicar que o envio do flag `FIN` não encerra a escuta forçadamente por conta do conceito *half-close* em ferramentas como o `netcat`. Isso me direcionou para fazer com que o `cliente.c` capturasse os `0 bytes` de leitura e desligasse organicamente o cliente com um `break`.
*   **Geração de Artefatos:** A estruturação final deste `README.md` e as regras de compilação da ferramenta `Makefile` foram consolidadas através de roteiros gerados diretamente pela IA a partir das minhas implementações em C.

## 6. Bônus Implementado: Thread Pool (+1,0)
A arquitetura do servidor foi aprimorada, substituindo o modelo de *thread-por-conexão* dinâmico por um padrão *Produtor-Consumidor* otimizado. 

**Funcionamento da Sincronização:**
Uma equipe de N=4 *worker threads* (consumidores) é criada no início da execução. A fila de pendências (`FilaConexoes`) foi implementada em um arquivo isolado como um *buffer* circular de capacidade máxima de 64 *sockets*. Ela é protegida internamente por um `pthread_mutex_t` para garantir a exclusão mútua e utiliza duas variáveis de condição (`pthread_cond_t nao_cheia` e `nao_vazia`) para sinalização sem espera ocupada.

A *thread* principal (`main`) executa o `accept()` e enfileira o socket. Caso ocorra um pico gigantesco de requisições e a fila lote, a *thread* principal aplicará uma *backpressure*, bloqueando-se (`pthread_cond_wait(&nao_cheia)`) até que os trabalhadores aliviem o gargalo. De forma inversa, se a casa estiver ociosa, os *workers* ficam dormindo na variável `nao_vazia` sem gastar CPU, preservando a performance do hardware.

**Gerenciamento de Gargalos: Backlog vs Fila da Aplicação**
É importante destacar a arquitetura de dupla fila adotada para absorver picos de requisições:
1.  **Backlog do Sistema Operacional (`listen`):** Configurado com tamanho 10. Esta é a fila em nível de Kernel (pilha TCP/IP) que segura os *handshakes* de conexões recém-chegadas que ainda não foram processadas pela chamada `accept()`.
2.  **Fila da Aplicação (`FilaConexoes`):** Configurada com tamanho 64. É o *buffer* do padrão Produtor-Consumidor no espaço de usuário.

Essa separação garante que o servidor não rejeite conexões (evitando o erro `ECONNREFUSED`) durante surtos de tráfego que ocorram no exato milissegundo em que a *thread* principal está ocupada transferindo um *socket* do Kernel para o *Thread Pool*.

**Encerramento Gracioso do Pool (`pthread_join`)**
Ao contrário do modelo *thread-por-conexão* original — em que cada *thread* era efêmera e podia ser apenas `detach`ada —, as *worker threads* do pool têm ciclo de vida atrelado ao processo inteiro, então o encerramento precisa ser coordenado explicitamente:
 
1.  Ao receber `SIGINT`/`SIGTERM`, o `main` sai do laço de `accept()` e chama `fila_encerrar()`, que liga uma *flag* `encerrando` protegida pelo mesmo *mutex* da fila e dá `pthread_cond_broadcast` na condvar `nao_vazia`. Isso acorda **todos** os *workers*, mesmo os que estão ociosos e bloqueados em `pthread_cond_wait` — um `pthread_cond_signal` sozinho poderia acordar só um deles.
2.  Cada *worker*, ao acordar, reavalia a condição de parada: se a fila está vazia e `encerrando` está ligada, `fila_desenfileirar` retorna o sentinela `-1` em vez de bloquear de novo, e o *worker* sai do laço (`worker_loop`) naturalmente.
3.  Só então o `main` chama `pthread_join` em cada uma das `NUM_WORKERS` *threads*, garantindo que nenhuma delas ainda esteja usando o *mutex*/*condvars* da fila.
4.  Somente depois de todos os `join`s retornarem é que `estado_global->mutex` é destruído, o segmento é desalocado (`shm_unlink`) e `fila_destruir()` chama `pthread_mutex_destroy`/`pthread_cond_destroy` na fila.
Essa ordem importa: destruir um *mutex* ou uma *condvar* enquanto ainda existem *threads* bloqueadas nela é comportamento indefinido pelo POSIX. Por isso as *worker threads* deixaram de ser `pthread_detach`adas — sem o `pthread_join`, o processo não tem como saber que todas terminaram antes de liberar os recursos que elas ainda podem estar usando.