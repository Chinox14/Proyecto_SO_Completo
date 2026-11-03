/*
 * test_secuencia.c — Pruebas del lector de archivos y del generador (Fase 6)
 */
#include "memoria.h"

#include <stdio.h>
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

static Secuencia s1, s2;

static void escribir(const char *ruta, const char *contenido)
{
    FILE *f = fopen(ruta, "w");
    fputs(contenido, f);
    fclose(f);
}

static void prueba_xorshift(void)
{
    /* Valor de referencia de xorshift32 (Marsaglia) con estado inicial 1:
     * 1 -> 270369 -> 67634689 -> 2647435461. Si alguien cambia el generador,
     * los resultados del informe dejan de ser reproducibles: esta prueba avisa. */
    puts("T1 xorshift32 da los valores de referencia");
    unsigned int e = 1;
    CHECK(aleatorio_siguiente(&e) == 270369u, "1er valor");
    CHECK(aleatorio_siguiente(&e) == 67634689u, "2do valor");
    CHECK(aleatorio_siguiente(&e) == 2647435461u, "3er valor");
}

static void prueba_reproducible(void)
{
    puts("T2 misma semilla -> misma secuencia; otra semilla -> otra");
    secuencia_generar(&s1, 42, 5000, 4, 40, 9);
    secuencia_generar(&s2, 42, 5000, 4, 40, 9);
    CHECK(s1.n == 5000 && s2.n == 5000, "debían generarse 5000 eventos");
    CHECK(memcmp(s1.eventos, s2.eventos, sizeof(Evento) * (size_t)s1.n) == 0,
          "la misma semilla dio secuencias distintas");
    secuencia_generar(&s2, 43, 5000, 4, 40, 9);
    CHECK(memcmp(s1.eventos, s2.eventos, sizeof(Evento) * (size_t)s1.n) != 0,
          "semillas distintas dieron la misma secuencia");
}

static void prueba_validez(void)
{
    /* Toda secuencia generada debe ser coherente: los tamaños en rango,
     * cada pid se asigna una sola vez y solo se libera si está vivo. */
    puts("T3 secuencias válidas para 50 semillas y 3 niveles de carga");
    static int estado[MAX_EVENTOS];   /* 0 nunca visto, 1 vivo, 2 terminado */
    const int cargas[] = {4, 8, 13};
    for (int c = 0; c < 3; c++) {
        for (unsigned int sem = 1; sem <= 50; sem++) {
            secuencia_generar(&s1, sem, 5000, 4, 40, cargas[c]);
            memset(estado, 0, sizeof estado);
            int vivos = 0, suma_vivos = 0;
            int fallos_antes = fallos;
            for (int k = 0; k < s1.n && fallos == fallos_antes; k++) {
                Evento e = s1.eventos[k];
                CHECK(e.pid >= 0 && e.pid < MAX_EVENTOS, "pid fuera de rango");
                if (e.tipo == EV_ASIGNAR) {
                    CHECK(e.tamano >= 4 && e.tamano <= 40, "tamaño %d fuera de [4,40]", e.tamano);
                    CHECK(estado[e.pid] == 0, "semilla %u: P%d asignado dos veces", sem, e.pid);
                    estado[e.pid] = 1;
                    vivos++;
                } else {
                    CHECK(estado[e.pid] == 1, "semilla %u: se libera P%d que no está vivo", sem, e.pid);
                    estado[e.pid] = 2;
                    vivos--;
                }
                suma_vivos += vivos;
            }
            /* El número de vivos debe oscilar cerca del objetivo */
            double prom = (double)suma_vivos / s1.n;
            CHECK(prom > cargas[c] * 0.6 && prom < cargas[c] * 1.4,
                  "carga %d, semilla %u: vivos promedio %.2f lejos del objetivo",
                  cargas[c], sem, prom);
        }
    }
}

static void prueba_archivo_valido(void)
{
    puts("T4 leer archivo con comentarios, espacios y líneas vacías");
    const char *ruta = "build/tmp_ok.txt";
    escribir(ruta, "# comentario\n\nA 1 12\n   A 2 5\nL 1\n\t# otro\nA 3 7   \n");
    CHECK(secuencia_cargar(&s1, ruta), "debía leerse sin error");
    CHECK(s1.n == 4, "esperaba 4 eventos, hay %d", s1.n);
    CHECK(s1.eventos[0].tipo == EV_ASIGNAR && s1.eventos[0].pid == 1 && s1.eventos[0].tamano == 12, "evento 0");
    CHECK(s1.eventos[2].tipo == EV_LIBERAR && s1.eventos[2].pid == 1, "evento 2");
    CHECK(s1.eventos[3].tamano == 7, "evento 3 con espacios al final");

    CHECK(secuencia_cargar(&s1, "datos/ejemplo_apuntes.txt") && s1.n == 13,
          "ejemplo_apuntes.txt debía tener 13 eventos");
    CHECK(secuencia_cargar(&s1, "datos/ejemplo_quiz.txt") && s1.n == 10,
          "ejemplo_quiz.txt debía tener 10 eventos");
}

static void prueba_archivo_invalido(void)
{
    puts("T5 rechazar archivos mal escritos (los mensajes de error son esperados)");
    struct { const char *contenido; const char *por_que; } malos[] = {
        {"A 1\n",         "asignación sin tamaño"},
        {"A 1 0\n",       "tamaño 0"},
        {"A 1 -5\n",      "tamaño negativo"},
        {"L 1 7\n",       "liberación con dato de más"},
        {"X 1 5\n",       "tipo desconocido"},
        {"A 1 5 9\n",     "asignación con dato de más"},
        {"A -1 5\n",      "pid negativo"},
        {"A 99999 5\n",   "pid demasiado grande"},
        {"A uno 5\n",     "pid no numérico"},
    };
    const char *ruta = "build/tmp_malo.txt";
    for (size_t i = 0; i < sizeof malos / sizeof malos[0]; i++) {
        escribir(ruta, malos[i].contenido);
        CHECK(!secuencia_cargar(&s1, ruta), "debía rechazar: %s", malos[i].por_que);
    }
    CHECK(!secuencia_cargar(&s1, "build/no_existe.txt"), "archivo inexistente");
}

int main(void)
{
    prueba_xorshift();
    prueba_reproducible();
    prueba_validez();
    prueba_archivo_valido();
    prueba_archivo_invalido();

    printf("\n%d comprobaciones, %d fallos\n", pruebas, fallos);
    return fallos == 0 ? 0 : 1;
}
