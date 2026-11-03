/*
 * secuencia.c — Secuencias de solicitudes: leer desde archivo o generar (Fase 6)
 *
 * La MISMA secuencia se aplica a las tres estrategias. Así, cualquier
 * diferencia en los resultados se debe a la estrategia y no a la entrada.
 */
#include "memoria.h"

#include <stdio.h>
#include <string.h>

/* ---------- Generador pseudoaleatorio portable ---------- */

/*
 * xorshift32 (Marsaglia, 2003). Tres desplazamientos y XOR: rápido y, sobre
 * todo, da EXACTAMENTE la misma secuencia en cualquier sistema operativo.
 */
unsigned int aleatorio_siguiente(unsigned int *estado)
{
    unsigned int x = *estado;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *estado = x;
    return x;
}

/* Entero uniforme en [a, b] */
static int aleatorio_entre(unsigned int *estado, int a, int b)
{
    return a + (int)(aleatorio_siguiente(estado) % (unsigned int)(b - a + 1));
}

/* Real uniforme en [0, 1) */
static double aleatorio_unitario(unsigned int *estado)
{
    return (aleatorio_siguiente(estado) >> 8) / 16777216.0;   /* 2^24 */
}

/* ---------- Lectura desde archivo ---------- */

bool secuencia_cargar(Secuencia *s, const char *ruta)
{
    FILE *f = fopen(ruta, "r");
    if (f == NULL) {
        fprintf(stderr, "No se pudo abrir '%s'\n", ruta);
        return false;
    }

    s->n = 0;
    char linea[256];
    int num_linea = 0;
    bool ok = true;

    while (fgets(linea, sizeof linea, f) != NULL) {
        num_linea++;

        /* Saltar espacios iniciales; ignorar vacías y comentarios */
        char *p = linea;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '#' || *p == '\n' || *p == '\r' || *p == '\0') {
            continue;
        }

        if (s->n >= MAX_EVENTOS) {
            fprintf(stderr, "%s:%d: más de %d eventos\n", ruta, num_linea, MAX_EVENTOS);
            ok = false;
            break;
        }

        char tipo;
        int pid, tam;
        char sobra[2];
        Evento e;

        if (sscanf(p, " %c %d %d %1s", &tipo, &pid, &tam, sobra) == 3 && tipo == 'A') {
            if (tam <= 0) {
                fprintf(stderr, "%s:%d: el tamaño debe ser > 0\n", ruta, num_linea);
                ok = false;
                break;
            }
            e = (Evento){EV_ASIGNAR, pid, tam};
        } else if (sscanf(p, " %c %d %1s", &tipo, &pid, sobra) == 2 && tipo == 'L') {
            e = (Evento){EV_LIBERAR, pid, 0};
        } else {
            fprintf(stderr, "%s:%d: línea no válida (use 'A <pid> <KB>' o 'L <pid>'): %s",
                    ruta, num_linea, linea);
            ok = false;
            break;
        }

        if (pid < 0 || pid >= MAX_EVENTOS) {
            fprintf(stderr, "%s:%d: pid %d fuera de [0, %d)\n", ruta, num_linea, pid, MAX_EVENTOS);
            ok = false;
            break;
        }
        s->eventos[s->n++] = e;
    }

    fclose(f);
    return ok;
}

/* ---------- Generación aleatoria reproducible ---------- */

void secuencia_generar(Secuencia *s, unsigned int semilla, int num_eventos,
                       int tam_min, int tam_max, int vivos_objetivo)
{
    /* xorshift no admite estado 0: se mezcla la semilla con una constante */
    unsigned int estado = semilla ^ 0x9E3779B9u;
    if (estado == 0) estado = 1;

    if (num_eventos > MAX_EVENTOS) num_eventos = MAX_EVENTOS;

    /* Procesos vivos SEGÚN EL GENERADOR (no sabe si el simulador los rechazó) */
    static int vivos[MAX_PROCESOS];
    int num_vivos = 0;
    int siguiente_pid = 0;

    s->n = 0;
    for (int k = 0; k < num_eventos; k++) {
        double p_liberar = (double)num_vivos / (num_vivos + vivos_objetivo);
        bool liberar = (num_vivos > 0 && aleatorio_unitario(&estado) < p_liberar)
                       || num_vivos == MAX_PROCESOS
                       || siguiente_pid == MAX_EVENTOS;

        if (liberar && num_vivos > 0) {
            /* Termina un proceso vivo cualquiera (uniforme) */
            int j = aleatorio_entre(&estado, 0, num_vivos - 1);
            s->eventos[s->n++] = (Evento){EV_LIBERAR, vivos[j], 0};
            vivos[j] = vivos[--num_vivos];   /* quitar sin dejar agujeros */
        } else {
            int tam = aleatorio_entre(&estado, tam_min, tam_max);
            s->eventos[s->n++] = (Evento){EV_ASIGNAR, siguiente_pid, tam};
            vivos[num_vivos++] = siguiente_pid++;
        }
    }
}
