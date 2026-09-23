/*
 * inseridor.c
 *
 * Programa para inserir novas instituicoes de ensino superior (dataset
 * e-MEC) em um arquivo CSV de destino, garantindo que a coluna
 * CODIGO_DA_IES permaneca sem valores duplicados. O arquivo de destino
 * permanece SEM ordenacao (novas entradas sao sempre inseridas no final).
 *
 * O programa implementa, para fins de demonstracao e validacao
 * experimental de complexidade, seis algoritmos:
 *
 *   SOLUCAO 1 (sobre o vetor NAO ordenado, como esta no destino):
 *     - busca sequencial iterativa
 *     - busca sequencial recursiva
 *
 *   SOLUCAO 2 (sobre uma copia ORDENADA em RAM):
 *     - mergesort iterativo (bottom-up)
 *     - mergesort recursivo (top-down)
 *     - busca binaria iterativa
 *     - busca binaria recursiva
 *
 * A decisao de aceitar ou rejeitar cada nova entrada (evitar duplicidade)
 * e tomada pela busca sequencial ITERATIVA sobre o vetor nao ordenado,
 * que e a que efetivamente reflete o arquivo de destino tal como o
 * enunciado especifica (nao ordenado). As demais cinco implementacoes
 * sao executadas EM PARALELO, sobre os MESMOS dados, apenas para fins de
 * instrumentacao e comparacao de complexidade -- elas nunca alteram o
 * resultado gravado no arquivo de destino.
 *
 * Uso:
 *   ./inseridor novas_entradas.csv destino.csv
 *
 * (O nome do arquivo de estatisticas nao e um argumento de linha de
 * comando, pois o enunciado desta atividade especifica apenas dois
 * argumentos. As metricas sao gravadas em "estatisticas_algoritmos.csv",
 * no diretorio de execucao, em modo "append" -- cada execucao do
 * programa acrescenta uma nova linha, permitindo acumular uma tabela de
 * experimentos com diferentes tamanhos de entrada.)
 *
 * Formato de cada linha do CSV (dataset e-MEC):
 *   CODIGO_DA_IES,NOME_DA_IES,SIGLA,CATEGORIA_DA_IES,COMUNITARIA,
 *   CONFESSIONAL,FILANTROPICA,ORGANIZACAO_ACADEMICA,
 *   CODIGO_MUNICIPIO_IBGE,MUNICIPIO,UF,SITUACAO_IES
 *
 * CODIGO_DA_IES e sempre o primeiro campo, puramente numerico e nunca
 * citado entre aspas -- por isso pode ser extraido com seguranca mesmo
 * que outros campos da linha (como NOME_DA_IES) contenham virgulas
 * internas entre aspas.
 *
 * Compilacao:
 *   gcc -Wall -Wextra -O2 -o inseridor inseridor.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAX_LINE 4096

/* =======================================================================
 * ESTRUTURAS DE DADOS
 * ======================================================================= */

/* Vetor dinamico NAO ordenado: espelha o arquivo de destino tal como ele
 * existe (ordem de insercao original). Usado pela Solucao 1. */
typedef struct {
    long  *codes;    /* CODIGO_DA_IES de cada registro, na ordem do arquivo */
    char **lines;    /* linha CSV completa correspondente a cada codigo    */
    int    count;
    int    capacity;
} UnsortedRegistry;

/* Vetor dinamico ORDENADO (crescente) mantido apenas em RAM, usado pela
 * Solucao 2 (mergesort + busca binaria). */
typedef struct {
    long *codes;
    int   count;
    int   capacity;
} SortedArray;

static void unsorted_init(UnsortedRegistry *r) {
    r->capacity = 256;
    r->count = 0;
    r->codes = (long *)  malloc(sizeof(long)  * (size_t) r->capacity);
    r->lines = (char **) malloc(sizeof(char *) * (size_t) r->capacity);
    if (!r->codes || !r->lines) {
        fprintf(stderr, "ERRO: falha de alocacao de memoria.\n");
        exit(EXIT_FAILURE);
    }
}

static void unsorted_add(UnsortedRegistry *r, long code, const char *line) {
    if (r->count >= r->capacity) {
        r->capacity *= 2;
        long  *tmp_codes = (long *)  realloc(r->codes, sizeof(long)  * (size_t) r->capacity);
        char **tmp_lines = (char **) realloc(r->lines, sizeof(char *) * (size_t) r->capacity);
        if (!tmp_codes || !tmp_lines) {
            fprintf(stderr, "ERRO: falha de realocacao de memoria.\n");
            exit(EXIT_FAILURE);
        }
        r->codes = tmp_codes;
        r->lines = tmp_lines;
    }
    r->codes[r->count] = code;
    size_t len = strlen(line);
    char *copy = (char *) malloc(len + 1);
    if (!copy) {
        fprintf(stderr, "ERRO: falha de alocacao de memoria.\n");
        exit(EXIT_FAILURE);
    }
    memcpy(copy, line, len + 1);
    r->lines[r->count] = copy;
    r->count++;
}

static void unsorted_free(UnsortedRegistry *r) {
    for (int i = 0; i < r->count; i++) free(r->lines[i]);
    free(r->lines);
    free(r->codes);
    r->lines = NULL;
    r->codes = NULL;
    r->count = 0;
    r->capacity = 0;
}

static void sorted_init(SortedArray *s, int initial_capacity) {
    s->capacity = initial_capacity > 0 ? initial_capacity : 256;
    s->count = 0;
    s->codes = (long *) malloc(sizeof(long) * (size_t) s->capacity);
    if (!s->codes) {
        fprintf(stderr, "ERRO: falha de alocacao de memoria.\n");
        exit(EXIT_FAILURE);
    }
}

static void sorted_ensure_capacity(SortedArray *s, int needed) {
    if (needed <= s->capacity) return;
    while (s->capacity < needed) s->capacity *= 2;
    long *tmp = (long *) realloc(s->codes, sizeof(long) * (size_t) s->capacity);
    if (!tmp) {
        fprintf(stderr, "ERRO: falha de realocacao de memoria.\n");
        exit(EXIT_FAILURE);
    }
    s->codes = tmp;
}

static void sorted_free(SortedArray *s) {
    free(s->codes);
    s->codes = NULL;
    s->count = 0;
    s->capacity = 0;
}

/* =======================================================================
 * UTILITARIOS DE LEITURA DE LINHA / CSV
 * ======================================================================= */

static void strip_newline(char *s) {
    size_t len = strlen(s);
    while (len > 0 && (s[len - 1] == '\n' || s[len - 1] == '\r')) {
        s[--len] = '\0';
    }
}

/* CODIGO_DA_IES e sempre o primeiro campo, puramente numerico. strtol
 * para automaticamente ao encontrar a primeira virgula. */
static long extract_code(const char *line) {
    return strtol(line, NULL, 10);
}

/* =======================================================================
 * SOLUCAO 1 -- BUSCA SEQUENCIAL (sobre o vetor NAO ordenado)
 * ======================================================================= */

/* Busca sequencial ITERATIVA. Retorna o indice do elemento se encontrado,
 * ou -1 caso contrario. Incrementa *comparisons a cada comparacao. */
static int seq_search_iterative(const long *arr, int n, long key, long *comparisons) {
    for (int i = 0; i < n; i++) {
        (*comparisons)++;
        if (arr[i] == key) {
            return i;
        }
    }
    return -1;
}

/* Busca sequencial RECURSIVA. Mesma semantica da versao iterativa. */
static int seq_search_recursive(const long *arr, int idx, int n, long key, long *comparisons) {
    if (idx >= n) {
        return -1; /* caso base: fim do vetor, nao encontrado */
    }
    (*comparisons)++;
    if (arr[idx] == key) {
        return idx; /* caso base: encontrado */
    }
    return seq_search_recursive(arr, idx + 1, n, key, comparisons); /* passo recursivo */
}

/* =======================================================================
 * SOLUCAO 2 -- MERGESORT (ordena uma copia do vetor em RAM)
 * ======================================================================= */

/* Intercala arr[left..mid] com arr[mid+1..right] usando "temp" como
 * buffer auxiliar. Conta comparacoes (uma por decisao de qual elemento
 * segue para a saida) e movimentacoes (uma por elemento copiado). */
static void merge_and_count(long *arr, int left, int mid, int right,
                             long *temp, long *comparisons, long *moves) {
    int i = left, j = mid + 1, k = left;

    while (i <= mid && j <= right) {
        (*comparisons)++;
        if (arr[i] <= arr[j]) {
            temp[k++] = arr[i++];
        } else {
            temp[k++] = arr[j++];
        }
        (*moves)++;
    }
    while (i <= mid)  { temp[k++] = arr[i++]; (*moves)++; }
    while (j <= right) { temp[k++] = arr[j++]; (*moves)++; }

    for (int x = left; x <= right; x++) {
        arr[x] = temp[x];
        (*moves)++; /* copia de volta do buffer auxiliar para arr[] */
    }
}

/* Mergesort RECURSIVO (top-down): divide o vetor ao meio recursivamente
 * ate subvetores de tamanho 1, depois intercala de volta subindo a
 * pilha de recursao. Profundidade de pilha: O(log n). */
static void mergesort_recursive(long *arr, int left, int right, long *temp,
                                 long *comparisons, long *moves) {
    if (left >= right) {
        return; /* caso base: subvetor de 0 ou 1 elemento, ja ordenado */
    }
    int mid = left + (right - left) / 2;
    mergesort_recursive(arr, left, mid, temp, comparisons, moves);
    mergesort_recursive(arr, mid + 1, right, temp, comparisons, moves);
    merge_and_count(arr, left, mid, right, temp, comparisons, moves);
}

/* Mergesort ITERATIVO (bottom-up): intercala subvetores de tamanho
 * crescente (1, 2, 4, 8, ...) sem nenhuma chamada recursiva.
 * Profundidade de pilha: O(1). */
static void mergesort_iterative(long *arr, int n, long *temp,
                                 long *comparisons, long *moves) {
    for (int width = 1; width < n; width *= 2) {
        for (int left = 0; left < n - width; left += 2 * width) {
            int mid   = left + width - 1;
            int right = left + 2 * width - 1;
            if (right > n - 1) right = n - 1;
            merge_and_count(arr, left, mid, right, temp, comparisons, moves);
        }
    }
}

/* =======================================================================
 * SOLUCAO 2 -- BUSCA BINARIA (sobre o vetor ordenado em RAM)
 * ======================================================================= */

/* Busca binaria ITERATIVA. Se encontrar, "*found" = 1 e retorna o indice
 * do elemento. Se nao encontrar, "*found" = 0 e retorna o INDICE DE
 * INSERCAO (posicao onde "key" deveria entrar para manter a ordem). */
static int bin_search_iterative(const long *arr, int n, long key,
                                 long *comparisons, int *found) {
    int low = 0, high = n - 1;
    *found = 0;
    while (low <= high) {
        int mid = low + (high - low) / 2;
        (*comparisons)++;
        if (arr[mid] == key) {
            *found = 1;
            return mid;
        }
        (*comparisons)++;
        if (arr[mid] < key) {
            low = mid + 1;
        } else {
            high = mid - 1;
        }
    }
    return low; /* posicao de insercao */
}

/* Busca binaria RECURSIVA. Mesma semantica da versao iterativa. */
static int bin_search_recursive(const long *arr, int low, int high, long key,
                                 long *comparisons, int *found) {
    if (low > high) {
        *found = 0;
        return low; /* caso base: nao encontrado; "low" e a posicao de insercao */
    }
    int mid = low + (high - low) / 2;
    (*comparisons)++;
    if (arr[mid] == key) {
        *found = 1;
        return mid; /* caso base: encontrado */
    }
    (*comparisons)++;
    if (arr[mid] < key) {
        return bin_search_recursive(arr, mid + 1, high, key, comparisons, found);
    } else {
        return bin_search_recursive(arr, low, mid - 1, key, comparisons, found);
    }
}

/* Insere "value" na posicao "pos" do vetor ordenado, deslocando os
 * elementos seguintes uma posicao a direita. Custo O(n - pos), contado
 * em "*moves". Isso NAO e uma busca -- e o custo estrutural de manter um
 * vetor ordenado, cobrado toda vez que uma nova entrada e aceita. */
static void sorted_insert_at(SortedArray *s, int pos, long value, long *moves) {
    sorted_ensure_capacity(s, s->count + 1);
    for (int i = s->count; i > pos; i--) {
        s->codes[i] = s->codes[i - 1];
        (*moves)++;
    }
    s->codes[pos] = value;
    s->count++;
}

/* =======================================================================
 * PROGRAMA PRINCIPAL
 * ======================================================================= */

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Uso: %s <novas_entradas.csv> <destino.csv>\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *src_path = argv[1];
    const char *dst_path = argv[2];
    const char *stats_path = "estatisticas_algoritmos.csv";

    char line[MAX_LINE];

    /* -------- 1. Carregamento do destino (vetor NAO ordenado) -------- */
    UnsortedRegistry destino;
    unsorted_init(&destino);

    char header_line[MAX_LINE] = "";
    int dest_exists = 0;

    FILE *dst_read = fopen(dst_path, "r");
    if (dst_read) {
        dest_exists = 1;
        if (fgets(line, sizeof(line), dst_read)) {
            strip_newline(line);
            strncpy(header_line, line, sizeof(header_line) - 1);
            header_line[sizeof(header_line) - 1] = '\0';
        }
        while (fgets(line, sizeof(line), dst_read)) {
            strip_newline(line);
            if (strlen(line) == 0) continue;
            long code = extract_code(line);
            unsorted_add(&destino, code, line);
        }
        fclose(dst_read);
    }
    if (!dest_exists) {
        strcpy(header_line,
               "CODIGO_DA_IES,NOME_DA_IES,SIGLA,CATEGORIA_DA_IES,COMUNITARIA,"
               "CONFESSIONAL,FILANTROPICA,ORGANIZACAO_ACADEMICA,"
               "CODIGO_MUNICIPIO_IBGE,MUNICIPIO,UF,SITUACAO_IES");
    }

    int n0 = destino.count; /* tamanho do destino ANTES do lote atual */

    /* -------- 2. Demonstracao do mergesort (Solucao 2) -------- */
    /* Duas copias independentes do estado inicial do destino sao
     * ordenadas: uma com mergesort iterativo, outra com mergesort
     * recursivo. Ambas operam sobre os MESMOS n0 elementos, permitindo
     * comparar comparacoes/movimentacoes entre as duas implementacoes. */
    long ms_iter_comparisons = 0, ms_iter_moves = 0;
    long ms_rec_comparisons  = 0, ms_rec_moves  = 0;

    long *copy_iter = (long *) malloc(sizeof(long) * (size_t)(n0 > 0 ? n0 : 1));
    long *copy_rec  = (long *) malloc(sizeof(long) * (size_t)(n0 > 0 ? n0 : 1));
    long *temp_buf  = (long *) malloc(sizeof(long) * (size_t)(n0 > 0 ? n0 : 1));
    if (!copy_iter || !copy_rec || !temp_buf) {
        fprintf(stderr, "ERRO: falha de alocacao de memoria.\n");
        return EXIT_FAILURE;
    }
    memcpy(copy_iter, destino.codes, sizeof(long) * (size_t) n0);
    memcpy(copy_rec,  destino.codes, sizeof(long) * (size_t) n0);

    if (n0 > 0) {
        mergesort_iterative(copy_iter, n0, temp_buf, &ms_iter_comparisons, &ms_iter_moves);
        mergesort_recursive(copy_rec, 0, n0 - 1, temp_buf, &ms_rec_comparisons, &ms_rec_moves);
    }

    /* Verificacao de sanidade: as duas ordenacoes devem produzir o mesmo
     * resultado (mesmo multiconjunto ordenado), ja que partem dos mesmos
     * dados. */
    int mismatch = 0;
    for (int i = 0; i < n0; i++) {
        if (copy_iter[i] != copy_rec[i]) { mismatch = 1; break; }
    }
    if (mismatch) {
        fprintf(stderr, "AVISO: mergesort iterativo e recursivo divergiram no resultado!\n");
    }

    /* O vetor ordenado "operacional" da Solucao 2 parte do resultado do
     * mergesort iterativo (escolha arbitraria -- os dois produzem o
     * mesmo vetor ordenado) e sera atualizado por insercao ordenada a
     * cada nova entrada aceita. */
    SortedArray ordenado;
    sorted_init(&ordenado, n0 > 0 ? n0 : 256);
    for (int i = 0; i < n0; i++) {
        ordenado.codes[i] = copy_iter[i];
    }
    ordenado.count = n0;

    free(copy_iter);
    free(copy_rec);
    free(temp_buf);

    /* -------- 3. Abertura do arquivo de origem -------- */
    FILE *src = fopen(src_path, "r");
    if (!src) {
        fprintf(stderr, "ERRO: nao foi possivel abrir o arquivo de origem '%s'\n", src_path);
        unsorted_free(&destino);
        sorted_free(&ordenado);
        return EXIT_FAILURE;
    }
    if (!fgets(line, sizeof(line), src)) {
        fprintf(stderr, "ERRO: arquivo de origem vazio ou invalido.\n");
        fclose(src);
        unsorted_free(&destino);
        sorted_free(&ordenado);
        return EXIT_FAILURE;
    }

    /* -------- 4. Abertura do destino para escrita -------- */
    FILE *dst = fopen(dst_path, dest_exists ? "a" : "w");
    if (!dst) {
        fprintf(stderr, "ERRO: nao foi possivel abrir o arquivo de destino '%s'\n", dst_path);
        fclose(src);
        unsorted_free(&destino);
        sorted_free(&ordenado);
        return EXIT_FAILURE;
    }
    if (!dest_exists) {
        fprintf(dst, "%s\n", header_line);
    }

    /* -------- 5. Processamento do lote de novas entradas -------- */
    long seq_iter_comparisons = 0;
    long seq_rec_comparisons  = 0;
    long bin_iter_comparisons = 0;
    long bin_rec_comparisons  = 0;
    long sorted_insert_moves  = 0; /* custo O(n) de manter o vetor ordenado */

    int inserted_count  = 0;
    int redundant_count = 0;
    int m_entradas       = 0; /* total de linhas lidas da origem */

    while (fgets(line, sizeof(line), src)) {
        strip_newline(line);
        if (strlen(line) == 0) continue;
        m_entradas++;

        long code = extract_code(line);

        /* ---- SOLUCAO 1: busca sequencial (decide a insercao) ---- */
        int idx_iter = seq_search_iterative(destino.codes, destino.count, code,
                                             &seq_iter_comparisons);

        /* Mesma busca, versao recursiva -- apenas instrumentacao, nao
         * influencia a decisao de insercao (deve concordar com a
         * iterativa, pois consultam o mesmo vetor no mesmo instante). */
        int idx_rec = seq_search_recursive(destino.codes, 0, destino.count, code,
                                            &seq_rec_comparisons);
        if ((idx_iter == -1) != (idx_rec == -1)) {
            fprintf(stderr, "AVISO: busca sequencial iterativa e recursiva divergiram para %ld\n", code);
        }

        /* ---- SOLUCAO 2: busca binaria (apenas instrumentacao) ---- */
        int found_iter_bs = 0, found_rec_bs = 0;
        int pos_iter = bin_search_iterative(ordenado.codes, ordenado.count, code,
                                             &bin_iter_comparisons, &found_iter_bs);
        int pos_rec  = bin_search_recursive(ordenado.codes, 0, ordenado.count - 1, code,
                                             &bin_rec_comparisons, &found_rec_bs);
        if (found_iter_bs != found_rec_bs) {
            fprintf(stderr, "AVISO: busca binaria iterativa e recursiva divergiram para %ld\n", code);
        }

        int found = (idx_iter != -1); /* decisao oficial: Solucao 1 (iterativa) */

        if (found) {
            printf("ERRO: codigo de IES redundante encontrado: %ld\n", code);
            redundant_count++;
        } else {
            /* aceita a nova entrada: grava no destino (sempre no final,
             * arquivo permanece NAO ordenado) */
            fprintf(dst, "%s\n", line);
            unsorted_add(&destino, code, line);
            inserted_count++;

            /* atualiza o vetor ordenado da Solucao 2, pagando o custo
             * O(n) de insercao na posicao correta (obtida pela busca
             * binaria iterativa, por convencao) */
            sorted_insert_at(&ordenado, pos_iter, code, &sorted_insert_moves);
        }
        (void) pos_rec; /* pos_rec e equivalente a pos_iter; mantido apenas para instrumentacao */
    }

    fclose(src);
    fclose(dst);

    /* -------- 6. Geracao do arquivo de estatisticas -------- */
    int stats_exists = (access(stats_path, F_OK) == 0);
    FILE *stats = fopen(stats_path, "a");
    if (!stats) {
        fprintf(stderr, "ERRO: nao foi possivel abrir '%s'\n", stats_path);
        unsorted_free(&destino);
        sorted_free(&ordenado);
        return EXIT_FAILURE;
    }
    if (!stats_exists) {
        fprintf(stats,
            "n_inicial_destino,m_novas_entradas,nomes_inseridos,nomes_redundantes,"
            "seqiter_comparacoes,seqrec_comparacoes,"
            "msiter_comparacoes,msiter_movimentacoes,msrec_comparacoes,msrec_movimentacoes,"
            "biniter_comparacoes,binrec_comparacoes,"
            "insercao_ordenada_movimentacoes\n");
    }
    fprintf(stats, "%d,%d,%d,%d,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld\n",
            n0, m_entradas, inserted_count, redundant_count,
            seq_iter_comparisons, seq_rec_comparisons,
            ms_iter_comparisons, ms_iter_moves, ms_rec_comparisons, ms_rec_moves,
            bin_iter_comparisons, bin_rec_comparisons,
            sorted_insert_moves);
    fclose(stats);

    unsorted_free(&destino);
    sorted_free(&ordenado);

    printf("\nResumo da execucao:\n");
    printf("  n inicial do destino ............ %d\n", n0);
    printf("  novas entradas processadas ....... %d\n", m_entradas);
    printf("  inseridas ......................... %d\n", inserted_count);
    printf("  redundantes ........................ %d\n", redundant_count);
    printf("  Solucao 1 - busca sequencial iterativa: %ld comparacoes\n", seq_iter_comparisons);
    printf("  Solucao 1 - busca sequencial recursiva: %ld comparacoes\n", seq_rec_comparisons);
    printf("  Solucao 2 - mergesort iterativo: %ld comparacoes, %ld movimentacoes\n", ms_iter_comparisons, ms_iter_moves);
    printf("  Solucao 2 - mergesort recursivo: %ld comparacoes, %ld movimentacoes\n", ms_rec_comparisons, ms_rec_moves);
    printf("  Solucao 2 - busca binaria iterativa: %ld comparacoes\n", bin_iter_comparisons);
    printf("  Solucao 2 - busca binaria recursiva: %ld comparacoes\n", bin_rec_comparisons);
    printf("  Solucao 2 - movimentacoes p/ manter vetor ordenado: %ld\n", sorted_insert_moves);

    return EXIT_SUCCESS;
}