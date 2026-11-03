/*
 * test_nucleo.c — Pruebas de memoria.c e invariantes.c (Fase 3)
 *
 * Las estrategias reales (estrategias.c) llegan en la Fase 4. Aquí se usa
 * una estrategia PROVISIONAL de prueba, definida en este archivo.
 */
#include "memoria.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

/* Verifica los invariantes y muestra el motivo si fallan */
#define CHECK_INV(m)                                              \
    do {                                                          \
        char motivo_[160];                                        \
        bool ok_ = memoria_verificar((m), motivo_, sizeof motivo_); \
        CHECK(ok_, "invariante roto: %s", motivo_);               \
    } while (0)

/* ---------- Estrategias provisionales (solo para probar el núcleo) ---------- */

/* Primer hueco que sirve (equivale a primer ajuste) */
static int prov_primero(const Memoria *m, int tam, int *rev)
{
    *rev = 0;
    for (int i = 0; i < m->n; i++) {
        (*rev)++;
        if (m->bloques[i].libre && m->bloques[i].tamano >= tam) return i;
    }
    return -1;
}

/* Estrategia DEFECTUOSA a propósito: devuelve siempre el bloque 0 */
static int prov_rota(const Memoria *m, int tam, int *rev)
{
    (void)m; (void)tam;
    *rev = 1;
    return 0;
}

/* Memoria declarada 'static' (más de 20 KB, mejor fuera de la pila) */
static Memoria m;

/* ---------- Pruebas ---------- */

static void prueba_iniciar(void)
{
    puts("T1  iniciar: un único hueco");
    memoria_iniciar(&m, 256, 1);
    CHECK(m.n == 1 && m.bloques[0].libre && m.bloques[0].tamano == 256,
          "esperaba | libre 256 |");
    CHECK_INV(&m);
}

static void prueba_division_y_redondeo(void)
{
    puts("T2  asignar con división del hueco y redondeo a la unidad");
    memoria_iniciar(&m, 256, 4);
    Resultado r = memoria_asignar(&m, 1, 10, prov_primero, NULL);
    CHECK(r == OK, "resultado %s", resultado_texto(r));
    CHECK(m.n == 2, "esperaba 2 bloques, hay %d", m.n);
    CHECK(m.bloques[0].tamano == 12 && m.bloques[0].solicitado == 10,
          "P1 debía recibir 12 KB (pidió 10, unidad 4); recibió %d",
          m.bloques[0].tamano);
    CHECK(m.bloques[1].libre && m.bloques[1].inicio == 12 && m.bloques[1].tamano == 244,
          "el hueco sobrante debía ser [12] libre 244");
    CHECK_INV(&m);
}

static void prueba_ajuste_exacto(void)
{
    puts("T3  asignar un hueco exacto (sin dividir)");
    memoria_iniciar(&m, 64, 1);
    CHECK(memoria_asignar(&m, 1, 64, prov_primero, NULL) == OK, "debía caber exacto");
    CHECK(m.n == 1 && !m.bloques[0].libre, "no debía quedar hueco de 0 KB");
    CHECK_INV(&m);
}

/* Prepara | P1 10 | P2 10 | P3 10 | P4 10 | libre 60 |  */
static void preparar_cuatro(void)
{
    memoria_iniciar(&m, 100, 1);
    for (int p = 1; p <= 4; p++) memoria_asignar(&m, p, 10, prov_primero, NULL);
}

static void prueba_fusion(void)
{
    puts("T4  liberar: los 4 casos de fusión de huecos");

    preparar_cuatro();                 /* P2 con ocupados a ambos lados */
    memoria_liberar(&m, 2);
    CHECK(m.n == 5 && m.bloques[1].libre && m.bloques[1].tamano == 10,
          "sin vecinos libres: debía quedar un hueco de 10 aislado");
    CHECK_INV(&m);

    preparar_cuatro();                 /* libre a la izquierda */
    memoria_liberar(&m, 1);
    memoria_liberar(&m, 2);
    CHECK(m.bloques[0].libre && m.bloques[0].tamano == 20 && m.n == 4,
          "fusión con la izquierda: esperaba | libre 20 | P3 | P4 | libre 60 |");
    CHECK_INV(&m);

    preparar_cuatro();                 /* libre a la derecha */
    memoria_liberar(&m, 4);
    CHECK(m.bloques[3].libre && m.bloques[3].tamano == 70 && m.n == 4,
          "fusión con la derecha: esperaba ... | P3 | libre 70 |");
    CHECK_INV(&m);

    preparar_cuatro();                 /* libres a ambos lados (pregunta 3 del quiz) */
    memoria_liberar(&m, 1);
    memoria_liberar(&m, 3);
    memoria_liberar(&m, 2);
    CHECK(m.bloques[0].libre && m.bloques[0].tamano == 30 && m.n == 3,
          "fusión con ambos: esperaba | libre 30 | P4 10 | libre 60 |");
    CHECK_INV(&m);
}

static void prueba_errores(void)
{
    puts("T5  errores: pid duplicado, inexistente y parámetros inválidos");
    memoria_iniciar(&m, 100, 1);
    memoria_asignar(&m, 1, 10, prov_primero, NULL);
    CHECK(memoria_asignar(&m, 1, 5, prov_primero, NULL) == ERROR_PID_DUPLICADO, "pid duplicado");
    CHECK(memoria_liberar(&m, 99) == ERROR_PID_INEXISTENTE, "pid inexistente");
    CHECK(memoria_asignar(&m, 2, 0, prov_primero, NULL) == ERROR_PARAMETRO_INVALIDO, "tamaño 0");
    CHECK(memoria_asignar(&m, -3, 5, prov_primero, NULL) == ERROR_PARAMETRO_INVALIDO, "pid negativo");
    CHECK(m.n == 2, "un error no debe modificar la memoria");
    CHECK_INV(&m);
}

static void prueba_tipos_de_rechazo(void)
{
    puts("T6  rechazo por falta de espacio vs. por fragmentación externa");
    /* | libre 10 | P2 10 | libre 10 | P4 10 | libre 10 |  -> 30 KB libres */
    memoria_iniciar(&m, 50, 1);
    for (int p = 1; p <= 5; p++) memoria_asignar(&m, p, 10, prov_primero, NULL);
    memoria_liberar(&m, 1);
    memoria_liberar(&m, 3);
    memoria_liberar(&m, 5);
    Resultado r1 = memoria_asignar(&m, 6, 25, prov_primero, NULL);
    Resultado r2 = memoria_asignar(&m, 7, 40, prov_primero, NULL);
    CHECK(r1 == RECHAZO_FRAGMENTACION, "25 KB con 30 libres en pedazos: dio %s", resultado_texto(r1));
    CHECK(r2 == RECHAZO_SIN_ESPACIO, "40 KB con 30 libres: dio %s", resultado_texto(r2));
    CHECK_INV(&m);
}

static void prueba_caso_quiz(void)
{
    puts("T7  caso del quiz (datos/ejemplo_quiz.txt) con primer ajuste provisional");
    memoria_iniciar(&m, 100, 1);
    int tam[] = {12, 5, 50, 5, 20, 8};
    for (int p = 1; p <= 6; p++) memoria_asignar(&m, p, tam[p - 1], prov_primero, NULL);
    memoria_liberar(&m, 1);
    memoria_liberar(&m, 3);
    memoria_liberar(&m, 5);
    printf("    antes:   "); memoria_imprimir(&m);
    int rev = 0;
    CHECK(memoria_asignar(&m, 7, 18, prov_primero, &rev) == OK, "P7 debía caber");
    printf("    después: "); memoria_imprimir(&m);
    CHECK(m.bloques[2].pid == 7 && m.bloques[2].inicio == 17,
          "primer ajuste debía poner P7 en el hueco de 50 (inicio 17)");
    CHECK(m.bloques[3].libre && m.bloques[3].tamano == 32, "debían sobrar 32 KB");
    CHECK(rev == 3, "primer ajuste revisa 3 bloques (12 no sirve, P2, 50 sí); revisó %d", rev);
    CHECK_INV(&m);
}

static void prueba_estrategia_rota(void)
{
    puts("T8  una estrategia defectuosa no corrompe la memoria");
    memoria_iniciar(&m, 100, 1);
    memoria_asignar(&m, 1, 10, prov_primero, NULL);   /* bloque 0 queda ocupado */
    CHECK(memoria_asignar(&m, 2, 5, prov_rota, NULL) == ERROR_ESTRATEGIA_INVALIDA,
          "debía detectar que el bloque 0 no es un hueco");
    CHECK_INV(&m);
}

static void prueba_limite_bloques(void)
{
    puts("T9  límite de bloques: error limpio, sin escribir fuera del arreglo");
    memoria_iniciar(&m, 4000, 1);
    Resultado r = OK;
    int pid = 0;
    while (r == OK) r = memoria_asignar(&m, pid++, 1, prov_primero, NULL);
    CHECK(r == ERROR_SIN_BLOQUES, "esperaba ERROR_SIN_BLOQUES, dio %s", resultado_texto(r));
    CHECK(m.n == MAX_BLOQUES, "debía llenarse exactamente hasta %d bloques (hay %d)",
          MAX_BLOQUES, m.n);
    CHECK_INV(&m);
}

static void prueba_detector(void)
{
    puts("T10 el verificador SÍ detecta memoria corrupta (prueba del probador)");
    char motivo[160];

    memoria_iniciar(&m, 100, 1);
    memoria_asignar(&m, 1, 10, prov_primero, NULL);
    m.bloques[0].libre = true; m.bloques[0].pid = -1; m.bloques[0].solicitado = 0;
    CHECK(!memoria_verificar(&m, motivo, sizeof motivo) && strncmp(motivo, "I4", 2) == 0,
          "no detectó huecos sin fundir (I4)");

    memoria_iniciar(&m, 100, 1);
    m.bloques[0].tamano = 90;
    CHECK(!memoria_verificar(&m, motivo, sizeof motivo) && strncmp(motivo, "I2", 2) == 0,
          "no detectó suma distinta del total (I2)");

    memoria_iniciar(&m, 100, 1);
    memoria_asignar(&m, 1, 10, prov_primero, NULL);
    m.bloques[1].inicio = 11;
    CHECK(!memoria_verificar(&m, motivo, sizeof motivo) && strncmp(motivo, "I1", 2) == 0,
          "no detectó bloques no contiguos (I1)");

    memoria_iniciar(&m, 100, 1);
    memoria_asignar(&m, 1, 10, prov_primero, NULL);
    memoria_asignar(&m, 2, 10, prov_primero, NULL);
    m.bloques[1].pid = 1;
    CHECK(!memoria_verificar(&m, motivo, sizeof motivo) && strncmp(motivo, "I5", 2) == 0,
          "no detectó pid repetido (I5)");

    memoria_iniciar(&m, 100, 4);
    memoria_asignar(&m, 1, 8, prov_primero, NULL);
    m.bloques[0].solicitado = 2;   /* 8 asignados para 2: desperdicio >= unidad */
    CHECK(!memoria_verificar(&m, motivo, sizeof motivo) && strncmp(motivo, "I6", 2) == 0,
          "no detectó fragmentación interna imposible (I6)");
}

static void prueba_estres(void)
{
    /* 20 000 operaciones aleatorias con semilla fija. Un "modelo sombra"
     * guarda por separado qué procesos deberían estar vivos y cuánto pidieron.
     * Después de cada operación se comparan memoria, modelo e invariantes. */
    puts("T11 estrés: 20 000 operaciones aleatorias, semilla 42, unidad 4");
    enum { PIDS = 200 };
    int vivo[PIDS] = {0};          /* KB pedidos por el pid, 0 si no está vivo */
    int fallos_antes = fallos;

    srand(42);
    memoria_iniciar(&m, 256, 4);
    for (int k = 0; k < 20000 && fallos == fallos_antes; k++) {
        int pid = rand() % PIDS;
        if (vivo[pid] == 0) {
            int t = 1 + rand() % 40;
            Resultado r = memoria_asignar(&m, pid, t, prov_primero, NULL);
            if (r == OK) vivo[pid] = t;
            CHECK(r == OK || r == RECHAZO_FRAGMENTACION || r == RECHAZO_SIN_ESPACIO,
                  "op %d: resultado inesperado %s", k, resultado_texto(r));
        } else {
            CHECK(memoria_liberar(&m, pid) == OK, "op %d: no liberó P%d", k, pid);
            vivo[pid] = 0;
        }
        CHECK_INV(&m);

        /* El modelo sombra y la memoria deben coincidir */
        int vivos_modelo = 0, vivos_mem = 0;
        for (int p = 0; p < PIDS; p++) vivos_modelo += (vivo[p] > 0);
        for (int i = 0; i < m.n; i++) {
            if (m.bloques[i].libre) continue;
            vivos_mem++;
            int p = m.bloques[i].pid;
            CHECK(p < PIDS && vivo[p] == m.bloques[i].solicitado,
                  "op %d: P%d tiene solicitado %d y el modelo dice %d",
                  k, p, m.bloques[i].solicitado, p < PIDS ? vivo[p] : -1);
        }
        CHECK(vivos_modelo == vivos_mem, "op %d: %d vivos en el modelo, %d en memoria",
              k, vivos_modelo, vivos_mem);
    }
}

int main(void)
{
    prueba_iniciar();
    prueba_division_y_redondeo();
    prueba_ajuste_exacto();
    prueba_fusion();
    prueba_errores();
    prueba_tipos_de_rechazo();
    prueba_caso_quiz();
    prueba_estrategia_rota();
    prueba_limite_bloques();
    prueba_detector();
    prueba_estres();

    printf("\n%d comprobaciones, %d fallos\n", pruebas, fallos);
    return fallos == 0 ? 0 : 1;
}
