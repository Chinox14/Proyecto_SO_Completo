/*
 * test_estrategias.c — Pruebas de primer, mejor y peor ajuste (Fase 4)
 *
 * 1. Casos conocidos: el quiz y el ejemplo de los apuntes del profesor.
 * 2. Casos borde: sin hueco, hueco exacto, empates.
 * 3. ORÁCULO: para cada búsqueda en 30 000 operaciones aleatorias, cada
 *    estrategia se compara con su DEFINICIÓN escrita de la forma más simple
 *    posible (huecos que sirven -> el primero / el mínimo / el máximo).
 */
#include "memoria.h"

#include <stdio.h>
#include <stdlib.h>

static int fallos = 0;
static int pruebas = 0;

#define CHECK(cond, ...)                                          \
    do {                                                          \
        pruebas++;                                                \
        if (!(cond)) {                                            \
            fallos++;                                             \
            printf("  FALLA (línea %d): ", __LINE__);             \
            printf(__VA_ARGS__);                                  \
            printf("\n");                                         \
        }                                                         \
    } while (0)

#define CHECK_INV(m)                                                  \
    do {                                                              \
        char motivo_[160];                                            \
        bool ok_ = memoria_verificar((m), motivo_, sizeof motivo_);   \
        CHECK(ok_, "invariante roto: %s", motivo_);                   \
    } while (0)

static const char *NOMBRE[] = {"Primer ajuste", "Mejor ajuste ", "Peor ajuste  "};
static const FnEstrategia ESTRATEGIA[] = {elegir_first_fit, elegir_best_fit, elegir_worst_fit};

static Memoria m;

/*
 * Construye una memoria con los huecos dados, separados por procesos de
 * 1 KB, y con un proceso "relleno" al final que ocupa el resto.
 * Así los huecos no se funden entre sí. Usa la propia API del núcleo.
 */
static void armar_huecos(int total, const int *huecos, int k)
{
    memoria_iniciar(&m, total, 1);
    int pid = 0;
    int usados = 0;
    for (int j = 0; j < k; j++) {
        memoria_asignar(&m, pid++, huecos[j], elegir_first_fit, NULL);  /* futuro hueco */
        memoria_asignar(&m, pid++, 1, elegir_first_fit, NULL);          /* separador    */
        usados += huecos[j] + 1;
    }
    if (total > usados) {
        memoria_asignar(&m, pid++, total - usados, elegir_first_fit, NULL);
    }
    for (int j = 0; j < k; j++) {
        memoria_liberar(&m, 2 * j);          /* liberar los futuros huecos */
    }
}

/* ---------- 1. Casos conocidos ---------- */

static void caso(const char *titulo, const int *huecos, int k, int pedido,
                 const int esperado_inicio[3], const int esperado_hueco[3])
{
    printf("%s\n", titulo);
    armar_huecos(100, huecos, k);
    printf("    mapa: "); memoria_imprimir(&m);
    for (int e = 0; e < 3; e++) {
        int rev;
        int idx = ESTRATEGIA[e](&m, pedido, &rev);
        int ini = idx < 0 ? -1 : m.bloques[idx].inicio;
        int tam = idx < 0 ? -1 : m.bloques[idx].tamano;
        printf("    %s -> hueco %2d (inicio %2d), sobra %2d, revisó %d bloques\n",
               NOMBRE[e], tam, ini, tam - pedido, rev);
        CHECK(ini == esperado_inicio[e] && tam == esperado_hueco[e],
              "%s: esperaba hueco %d en %d", NOMBRE[e], esperado_hueco[e], esperado_inicio[e]);
    }
    CHECK_INV(&m);
}

/* ---------- 2. Casos borde ---------- */

static void prueba_bordes(void)
{
    puts("T3 casos borde");
    int rev;

    const int chicos[] = {5, 8, 3};
    armar_huecos(100, chicos, 3);
    for (int e = 0; e < 3; e++) {
        CHECK(ESTRATEGIA[e](&m, 9, &rev) == -1, "%s: ningún hueco sirve, debía dar -1", NOMBRE[e]);
        CHECK(rev == m.n, "%s: sin éxito debe revisar los %d bloques, revisó %d",
              NOMBRE[e], m.n, rev);
    }

    const int exacto[] = {30, 10, 20};   /* pedido 10: hay hueco exacto */
    armar_huecos(100, exacto, 3);
    int i_exacto = elegir_best_fit(&m, 10, &rev);
    CHECK(i_exacto >= 0 && m.bloques[i_exacto].tamano == 10,
          "mejor ajuste debía elegir el hueco exacto de 10");
    int rev_exacto;
    elegir_best_fit(&m, 10, &rev_exacto);
    CHECK(rev_exacto < m.n, "con hueco exacto, mejor ajuste debía cortar la búsqueda (revisó %d de %d)",
          rev_exacto, m.n);

    const int empate[] = {15, 20, 15, 20};
    armar_huecos(100, empate, 4);
    int i_best  = elegir_best_fit(&m, 12, &rev);
    int i_worst = elegir_worst_fit(&m, 12, &rev);
    CHECK(m.bloques[i_best].tamano == 15 && m.bloques[i_best].inicio == 0,
          "empate en mejor ajuste: debía ganar el 15 de menor dirección");
    CHECK(m.bloques[i_worst].tamano == 20 && m.bloques[i_worst].inicio == 16,
          "empate en peor ajuste: debía ganar el 20 de menor dirección (inicio 16)");

    memoria_iniciar(&m, 64, 1);           /* memoria vacía: un solo hueco */
    for (int e = 0; e < 3; e++) {
        CHECK(ESTRATEGIA[e](&m, 64, &rev) == 0, "%s: debía usar el único hueco", NOMBRE[e]);
    }
}

/* ---------- 3. Oráculo: definiciones directas ---------- */

/* 0 = primer, 1 = mejor, 2 = peor. Escrito sin atajos ni optimizaciones. */
static int oraculo(int estrategia, int tam)
{
    int idx_sirven[MAX_BLOQUES];
    int k = 0;
    for (int i = 0; i < m.n; i++) {
        if (m.bloques[i].libre && m.bloques[i].tamano >= tam) idx_sirven[k++] = i;
    }
    if (k == 0) return -1;
    int r = idx_sirven[0];
    for (int j = 1; j < k; j++) {
        int t = m.bloques[idx_sirven[j]].tamano;
        if (estrategia == 1 && t < m.bloques[r].tamano) r = idx_sirven[j];
        if (estrategia == 2 && t > m.bloques[r].tamano) r = idx_sirven[j];
    }
    return r;
}

static void prueba_oraculo(void)
{
    puts("T4 oráculo: 3 estrategias x 3 unidades x 10 000 operaciones aleatorias");
    enum { PIDS = 150 };
    const int unidades[] = {1, 4, 16};

    for (int u = 0; u < 3; u++) {
        for (int e = 0; e < 3; e++) {
            int vivo[PIDS] = {0};
            int fallos_antes = fallos;
            long revisados_total = 0;
            int busquedas = 0;

            srand(2026);  /* MISMA semilla para todas: misma secuencia */
            memoria_iniciar(&m, MEMORIA_DEFECTO, unidades[u]);

            for (int op = 0; op < 10000 && fallos == fallos_antes; op++) {
                int pid = rand() % PIDS;
                int t = 1 + rand() % 48;
                if (!vivo[pid]) {
                    int tam = ((t + unidades[u] - 1) / unidades[u]) * unidades[u];
                    int rev;
                    int esperado = oraculo(e, tam);
                    int real = ESTRATEGIA[e](&m, tam, &rev);
                    CHECK(real == esperado, "%s unidad %d op %d: eligió %d, la definición dice %d",
                          NOMBRE[e], unidades[u], op, real, esperado);
                    CHECK(rev >= 1 && rev <= m.n, "revisados fuera de rango: %d", rev);
                    revisados_total += rev;
                    busquedas++;
                    if (memoria_asignar(&m, pid, t, ESTRATEGIA[e], NULL) == OK) vivo[pid] = 1;
                } else {
                    memoria_liberar(&m, pid);
                    vivo[pid] = 0;
                }
                CHECK_INV(&m);
            }
            printf("    unidad %2d  %s  promedio revisados por búsqueda: %.2f\n",
                   unidades[u], NOMBRE[e], (double)revisados_total / busquedas);
        }
    }
}

int main(void)
{
    /* Separadores de 1 KB: huecos del quiz en 0, 13 y 64 */
    const int quiz[] = {12, 50, 20};
    caso("T1 quiz Fase 1: huecos 12, 50, 20; petición 18",
         quiz, 3, 18, (int[]){13, 64, 13}, (int[]){50, 20, 50});

    /* Huecos de los apuntes en 0, 16, 21 y 42 (igual que datos/ejemplo_apuntes.txt) */
    const int apuntes[] = {15, 4, 20, 13};
    caso("T2 apuntes del profesor: huecos 15, 4, 20, 13; petición 12",
         apuntes, 4, 12, (int[]){0, 42, 21}, (int[]){15, 13, 20});

    prueba_bordes();
    prueba_oraculo();

    printf("\n%d comprobaciones, %d fallos\n", pruebas, fallos);
    return fallos == 0 ? 0 : 1;
}
