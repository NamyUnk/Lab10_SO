/*
 * Lab 10 - Caching
 * Exercício 2: Cache-aside caching
 *
 * Programa multi-threaded com três threads:
 *   a) cliente: lê o log.txt ciclicamente (loop infinito) e lê/escreve
 *               no cache e/ou no banco de dados
 *   b) cache:   única thread que acessa o cache
 *   c) bd:      única thread que acessa o banco de dados
 *
 * Banco de dados: vetor de 1000 entradas (iniciado com valores 1..1000).
 * Cache: no máximo 100 entradas válidas; quando está cheio, remove a
 *        entrada mais antiga (menor timestamp).
 *
 * Política Cache-aside:
 *   - Leitura: o cliente consulta o cache primeiro. Se tem (hit), retorna.
 *              Se não tem (miss), busca no banco e salva no cache.
 *   - Escrita: o cliente escreve só no banco de dados.
 *
 * Ctrl-Z (SIGTSTP): limpa todo o cache, mas o programa continua
 * acessando o banco de dados.
 *
 * Compilação: gcc -Wall -o cache_aside cache_aside.c -lpthread
 * Execução:   ./cache_aside        (precisa do log.txt na mesma pasta)
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>

/* ==================== Código do slide "Brincando de Cache" ==================== */

#include <time.h>
typedef struct {
      int key;      // Integer in the range 0..999
      int value;    // Integer value
      time_t timestamp; // time when entry was written/accessed
} CacheEntry;

/* ============= Código do slide "Acessos Aleatórios a key = {0..999}" ============= */

typedef enum {
   READ, WRITE
} OpType;

/* ===================== Código do slide "Consulta ao Cache" ===================== */

#include <time.h>
#define CACHE_SIZE 1000
#define CACHE_HIT 1
#define CACHE_MISS 0

// O Cache é um vetor direto de CACHE_SIZE posições
CacheEntry cache[CACHE_SIZE];

// Uso de vetor auxiliar para indicar se entrada possui um dado válido (1) ou está vazio (0)
int is_valid[CACHE_SIZE] = {0};

int get(int key, int* out_value) {
   if (key < 0 || key >= CACHE_SIZE)   return CACHE_MISS;

    // Verifica se o slot correspondente à chave contém um registro válido
    if (is_valid[key]) {
        // CACHE HIT: Atualiza parâmetro de saída out_value com o valor guardado
        *out_value = cache[key].value;

    // Atualiza o timestamp para registrar o momento do último acesso (útil para políticas como LRU)
    cache[key].timestamp = time(NULL);

      return CACHE_HIT;
    }
    // CACHE MISS: A posição existe no array, mas está vazia/inválida
    return CACHE_MISS;
}

/* ============ Código dos slides "Leitura/Escrita (lenta) do Banco de Dados" ============ */

#define DB_SIZE 1000   // tamanho do banco (usado no slide, mas não definido lá)

#define DB_SUCCESS 1
#define DB_ERROR 0
#define DELAY 10

int banco_de_dados[DB_SIZE];

/* Procedimento para simular a LEITURA no Banco de Dados.
 * @return DB_SUCCESS (1) se a leitura foi bem-sucedida, DB_ERROR (0) se a chave for inválida
 */

int db_read(int key, int* out_value) {
   // Validação de segurança dos limites do banco de dados
   if (key < 0 || key >= 1000) {
       printf("[DB-ERRO] Tentativa de leitura em chave inválida: %d\n", key);
       return DB_ERROR;
   }
   // Copia o valor do banco de dados para o ponteiro de saída
   *out_value = banco_de_dados[key];

    printf("[DB-READ] Lendo do Banco -> Chave: %d, Valor: %d\n", key, *out_value);
    sleep(DELAY);
    return DB_SUCCESS;
}

 /**
  * Procedimento para simular a ESCRITA / ATUALIZAÇÃO no Banco de Dados.
  */
 int db_write(int key, int new_value) {
     // Validação de segurança dos limites do banco de dados
     if (key < 0 || key >= 1000) {
         printf("[DB-ERRO] Tentativa de escrita em chave inválida: %d\n", key);
         return DB_ERROR;
     }

 // Atualiza o registro no banco de dados com o novo valor
    banco_de_dados[key] = new_value;
    sleep(DELAY);

 printf("[DB-WRITE] Gravando no Banco -> Chave: %d, Novo Valor: %d\n", key, new_value);
    return DB_SUCCESS;
 }

/* ============================ Fim do código dos slides ============================ */


#define CACHE_MAX_ENTRADAS 100     // o cache guarda no máximo 100 entradas válidas
#define INTERVALO_ESTATISTICAS 10  // imprime estatísticas a cada 10 operações

int num_entradas = 0;                    // quantas entradas válidas o cache tem agora
volatile sig_atomic_t limpar_cache = 0;  // ligado pelo tratador do Ctrl-Z


/* ------------------------------ Funções do cache ------------------------------ */

// Remove do cache a entrada válida com o timestamp mais antigo
void remover_mais_antiga(void) {
    int mais_antiga = -1;

    for (int i = 0; i < CACHE_SIZE; i++) {
        if (is_valid[i] &&
            (mais_antiga == -1 || cache[i].timestamp < cache[mais_antiga].timestamp)) {
            mais_antiga = i;
        }
    }

    if (mais_antiga != -1) {
        is_valid[mais_antiga] = 0;
        num_entradas--;
        printf("[CACHE-REMOVE] Cache cheio, removendo a entrada mais antiga -> Chave: %d\n",
               mais_antiga);
    }
}

// Guarda (key, value) no cache. Se o cache estiver cheio, remove a entrada mais antiga.
void put(int key, int value) {
    if (key < 0 || key >= CACHE_SIZE) return;

    if (!is_valid[key]) {
        if (num_entradas == CACHE_MAX_ENTRADAS) {
            remover_mais_antiga();
        }
        num_entradas++;
    }

    cache[key].key = key;
    cache[key].value = value;
    cache[key].timestamp = time(NULL);
    is_valid[key] = 1;

    printf("[CACHE-PUT] Guardando no Cache -> Chave: %d, Valor: %d (entradas: %d)\n",
           key, value, num_entradas);
}

// Invalida todas as entradas do cache (usado quando chega o Ctrl-Z)
void esvaziar_cache(void) {
    int removidas = num_entradas;

    for (int i = 0; i < CACHE_SIZE; i++) {
        is_valid[i] = 0;
    }
    num_entradas = 0;

    printf("[CACHE] Ctrl-Z: cache limpo (%d entradas removidas)\n", removidas);
}


/* ------------------- Comunicação entre as threads (canais) -------------------
 * Cada thread servidora (cache e bd) tem um canal. Quem quer um serviço
 * coloca o pedido no canal e espera a resposta.
 */

// Pedidos que o cliente faz para a thread do cache
#define CACHE_PEGAR   0   // get(): consulta o cache
#define CACHE_GUARDAR 1   // put(): replica no cache um dado lido do banco

typedef struct {
    pthread_mutex_t mutex;
    pthread_cond_t  cond;
    int tem_pedido;     // 1 = há um pedido sendo atendido
    int tem_resposta;   // 1 = o servidor já respondeu
    int op;             // operação pedida
    int key;
    int value;
    int status;         // CACHE_HIT/CACHE_MISS ou DB_SUCCESS/DB_ERROR
} Canal;

Canal canal_cache;
Canal canal_bd;

void iniciar_canal(Canal* c) {
    pthread_mutex_init(&c->mutex, NULL);
    pthread_cond_init(&c->cond, NULL);
    c->tem_pedido = 0;
    c->tem_resposta = 0;
}

// Usado por quem pede o serviço: envia o pedido e espera a resposta
void enviar_pedido(Canal* c, int op, int key, int value, int* out_value, int* status) {
    pthread_mutex_lock(&c->mutex);

    while (c->tem_pedido || c->tem_resposta)   // espera o canal ficar livre
        pthread_cond_wait(&c->cond, &c->mutex);

    c->op = op;
    c->key = key;
    c->value = value;
    c->tem_pedido = 1;
    pthread_cond_broadcast(&c->cond);

    while (!c->tem_resposta)                    // espera a resposta
        pthread_cond_wait(&c->cond, &c->mutex);

    *out_value = c->value;
    *status = c->status;
    c->tem_resposta = 0;
    pthread_cond_broadcast(&c->cond);

    pthread_mutex_unlock(&c->mutex);
}

// Usado pela thread servidora: espera chegar um pedido
void receber_pedido(Canal* c, int* op, int* key, int* value) {
    pthread_mutex_lock(&c->mutex);

    while (!c->tem_pedido || c->tem_resposta)
        pthread_cond_wait(&c->cond, &c->mutex);

    *op = c->op;
    *key = c->key;
    *value = c->value;

    pthread_mutex_unlock(&c->mutex);
}

// Usado pela thread servidora: devolve a resposta para quem pediu
void responder(Canal* c, int value, int status) {
    pthread_mutex_lock(&c->mutex);

    c->value = value;
    c->status = status;
    c->tem_pedido = 0;
    c->tem_resposta = 1;
    pthread_cond_broadcast(&c->cond);

    pthread_mutex_unlock(&c->mutex);
}


/* ---------------------------------- Threads ---------------------------------- */

// c) Thread que acessa o banco de dados
void* thread_bd(void* arg) {
    int op, key, value, status;

    while (1) {
        receber_pedido(&canal_bd, &op, &key, &value);

        if (op == READ) {
            status = db_read(key, &value);
        } else {
            status = db_write(key, value);
        }

        responder(&canal_bd, value, status);
    }
    return NULL;
}

// b) Thread que acessa o cache
void* thread_cache(void* arg) {
    int op, key, value, status;

    while (1) {
        receber_pedido(&canal_cache, &op, &key, &value);

        // Se chegou um Ctrl-Z, limpa todo o cache antes de atender o pedido
        if (limpar_cache) {
            esvaziar_cache();
            limpar_cache = 0;
        }

        if (op == CACHE_PEGAR) {
            status = get(key, &value);
            if (status == CACHE_HIT)
                printf("[CACHE-HIT] Chave: %d, Valor: %d\n", key, value);
            else
                printf("[CACHE-MISS] Chave: %d\n", key);
        } else {   // CACHE_GUARDAR
            put(key, value);
            status = CACHE_HIT;
        }

        responder(&canal_cache, value, status);
    }
    return NULL;
}

// a) Thread cliente: lê o log ciclicamente e faz os pedidos (Cache-aside)
void* thread_cliente(void* arg) {
    FILE* log = fopen("log.txt", "r");
    if (log == NULL) {
        perror("Erro ao abrir log.txt");
        exit(1);
    }

    char linha[64];
    int key, value, status, lixo;
    long num_op = 0, leituras = 0, escritas = 0, hits = 0, misses = 0;
    time_t inicio = time(NULL);

    while (1) {
        // Lê o log ciclicamente: ao chegar no fim, volta para o início
        if (fgets(linha, sizeof(linha), log) == NULL) {
            printf("[CLIENTE] Fim do log.txt, recomeçando do início\n");
            rewind(log);
            continue;
        }

        time_t t0 = time(NULL);

        if (sscanf(linha, "R, %d", &key) == 1) {
            // LEITURA: consulta o cache primeiro
            leituras++;
            printf("\n[CLIENTE] Operação %ld: R, %d\n", num_op + 1, key);

            enviar_pedido(&canal_cache, CACHE_PEGAR, key, 0, &value, &status);

            if (status == CACHE_HIT) {
                hits++;
            } else {
                // MISS: busca no banco e replica o dado no cache
                misses++;
                enviar_pedido(&canal_bd, READ, key, 0, &value, &status);
                if (status == DB_SUCCESS)
                    enviar_pedido(&canal_cache, CACHE_GUARDAR, key, value, &lixo, &lixo);
            }
            printf("[CLIENTE] Valor lido: %d\n", value);

        } else if (sscanf(linha, "W, %d, %d", &value, &key) == 2) {
            // ESCRITA: no Cache-aside vai direto para o banco de dados
            escritas++;
            printf("\n[CLIENTE] Operação %ld: W, %d, %d\n", num_op + 1, value, key);

            enviar_pedido(&canal_bd, WRITE, key, value, &lixo, &status);

        } else {
            continue;   // linha em formato inesperado
        }

        num_op++;
        printf("[CLIENTE] Tempo da operação: %.0f s\n", difftime(time(NULL), t0));

        if (num_op % INTERVALO_ESTATISTICAS == 0) {
            double total = difftime(time(NULL), inicio);
            printf("\n[ESTATÍSTICAS Cache-aside] operações: %ld | leituras: %ld (hits: %ld, misses: %ld) "
                   "| escritas: %ld | taxa de hit: %.1f%% | tempo total: %.0f s | média: %.2f s/op\n",
                   num_op, leituras, hits, misses, escritas,
                   leituras > 0 ? 100.0 * hits / leituras : 0.0,
                   total, total / num_op);
        }
    }
    return NULL;
}


/* -------------------------------- Ctrl-Z e main -------------------------------- */

// Tratador do Ctrl-Z (SIGTSTP): avisa a thread do cache que ela deve limpar tudo
void trata_ctrl_z(int sig) {
    limpar_cache = 1;
    printf("\n[SINAL] Ctrl-Z recebido: o cache será limpo\n");
}

int main(void) {
    pthread_t t_cliente, t_cache, t_bd;
    sigset_t mascara;

    // Inicialmente todos os valores do banco de dados estão escritos com valores {1..1000}
    for (int i = 0; i < DB_SIZE; i++) {
        banco_de_dados[i] = i + 1;
    }

    iniciar_canal(&canal_cache);
    iniciar_canal(&canal_bd);

    signal(SIGTSTP, trata_ctrl_z);

    // Bloqueia o SIGTSTP antes de criar as threads (elas herdam o bloqueio).
    // Assim o Ctrl-Z é tratado só pelo main e não interrompe o sleep(DELAY)
    // da thread do banco de dados.
    sigemptyset(&mascara);
    sigaddset(&mascara, SIGTSTP);
    pthread_sigmask(SIG_BLOCK, &mascara, NULL);

    pthread_create(&t_bd, NULL, thread_bd, NULL);
    pthread_create(&t_cache, NULL, thread_cache, NULL);
    pthread_create(&t_cliente, NULL, thread_cliente, NULL);

    // O main volta a aceitar o SIGTSTP e fica só esperando sinais
    pthread_sigmask(SIG_UNBLOCK, &mascara, NULL);

    printf("Cache-aside iniciado (PID %d). Ctrl-Z limpa o cache, Ctrl-C encerra.\n", getpid());

    while (1) {
        pause();
    }
    return 0;
}
