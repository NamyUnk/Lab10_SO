/*
 * Lab 10 - Caching
 * Exercício 1: log_gen.c
 *
 * Usa a função gerar_operacoes(int n) do slide (sem mudanças) para
 * escrever 10.000 operações no arquivo log.txt.
 *
 * Como gerar_operacoes() escreve com printf (na saída padrão), o main
 * redireciona a saída padrão para o arquivo log.txt com freopen()
 * antes de chamar a função.
 *
 * Compilação: gcc -Wall -o log_gen log_gen.c
 * Execução:   ./log_gen
 */
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

/* ========== Código do slide "Acessos Aleatórios a key = {0..999}" ========== */

#define MAX_KEY 999
#define MIN_KEY 0

typedef enum {
   READ, WRITE
} OpType;

// Função que gera e exibe N operações aleatórias
void gerar_operacoes(int n) {
   // Inicializa o gerador de números aleatórios com o tempo atual
   srand(time(NULL));
   for (int i = 0; i < n; i++) {
       // Escolhe aleatoriamente entre READ (0) e WRITE (1)
       OpType op = rand() % 2;

       // Gera key aleatória no intervalo de 0 a 999
       int key = (rand() % (MAX_KEY - MIN_KEY + 1)) + MIN_KEY;

       if (op == READ) {
           // Formato pedido: R, key
           printf("R, %d\n", key);
       } else {
           // Gera um valor inteiro aleatório qualquer (ex: entre 10 e 5000)
           int value = (rand() % 4991) + 10;

           // Formato pedido: W, value, key
           printf("W, %d, %d\n", value, key);
       }
   }
}

/* ============================ Fim do código do slide ============================ */

#define NUM_OPERACOES 10000

int main(void) {
    // Redireciona a saída padrão (printf) para o arquivo log.txt
    if (freopen("log.txt", "w", stdout) == NULL) {
        perror("Erro ao abrir log.txt");
        exit(1);
    }

    gerar_operacoes(NUM_OPERACOES);

    fclose(stdout);

    // stdout agora é o arquivo, então a confirmação vai para stderr
    fprintf(stderr, "%d operacoes gravadas em log.txt\n", NUM_OPERACOES);
    return 0;
}
