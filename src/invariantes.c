/*
 * invariantes.c — Comprueba que la memoria simulada es consistente.
 *
 * Se llama después de CADA operación. Si algo se rompe, el simulador lo
 * informa en lugar de seguir y producir métricas falsas.
 */
#include "memoria.h"

#include <stdio.h>
#include <stdlib.h>

/* Escribe el motivo del fallo (si el llamador pidió uno) y devuelve false */
#define FALLA(...)                                              \
    do {                                                        \
        if (motivo != NULL && tam_motivo > 0) {                 \
            snprintf(motivo, (size_t)tam_motivo, __VA_ARGS__);  \
        }                                                       \
        return false;                                           \
    } while (0)

static int comparar_int(const void *a, const void *b)
{
    int x = *(const int *)a, y = *(const int *)b;
    return (x > y) - (x < y);
}

bool memoria_verificar(const Memoria *m, char *motivo, int tam_motivo)
{
    if (m->n < 1 || m->n > MAX_BLOQUES) {
        FALLA("I2: cantidad de bloques fuera de rango (n=%d)", m->n);
    }
    if (m->bloques[0].inicio != 0) {
        FALLA("I2: el primer bloque empieza en %d, no en 0", m->bloques[0].inicio);
    }

    int suma = 0;
    int pids[MAX_BLOQUES];
    int num_pids = 0;

    for (int i = 0; i < m->n; i++) {
        const Bloque *b = &m->bloques[i];

        /* I3: tamaño positivo y múltiplo de la unidad */
        if (b->tamano <= 0) {
            FALLA("I3: bloque %d con tamaño %d", i, b->tamano);
        }
        if (b->tamano % m->unidad != 0) {
            FALLA("I3: bloque %d de %d KB no es múltiplo de la unidad %d",
                  i, b->tamano, m->unidad);
        }

        /* I1: contiguos, sin huecos ni solapamientos entre bloques */
        if (i + 1 < m->n && m->bloques[i + 1].inicio != b->inicio + b->tamano) {
            FALLA("I1: bloque %d termina en %d pero el siguiente empieza en %d",
                  i, b->inicio + b->tamano, m->bloques[i + 1].inicio);
        }

        /* I4: dos huecos seguidos significan que faltó fundirlos */
        if (b->libre && i + 1 < m->n && m->bloques[i + 1].libre) {
            FALLA("I4: huecos adyacentes sin fundir en %d y %d", i, i + 1);
        }

        if (b->libre) {
            /* I7: un hueco no tiene dueño */
            if (b->pid != -1 || b->solicitado != 0) {
                FALLA("I7: hueco %d con pid=%d o solicitado=%d",
                      i, b->pid, b->solicitado);
            }
        } else {
            /* I7: un ocupado sí tiene dueño */
            if (b->pid < 0) {
                FALLA("I7: bloque ocupado %d con pid inválido %d", i, b->pid);
            }
            /* I6: la fragmentación interna es menor que una unidad */
            if (b->solicitado <= 0 || b->solicitado > b->tamano ||
                b->tamano - b->solicitado >= m->unidad) {
                FALLA("I6: P%d tiene %d KB para %d solicitados (unidad %d)",
                      b->pid, b->tamano, b->solicitado, m->unidad);
            }
            pids[num_pids++] = b->pid;
        }
        suma += b->tamano;
    }

    /* I2: los bloques cubren exactamente la memoria total */
    if (suma != m->total) {
        FALLA("I2: los bloques suman %d KB y la memoria tiene %d", suma, m->total);
    }

    /* I5: pids únicos (se ordenan y se comparan vecinos: O(n log n)) */
    qsort(pids, (size_t)num_pids, sizeof(int), comparar_int);
    for (int k = 1; k < num_pids; k++) {
        if (pids[k] == pids[k - 1]) {
            FALLA("I5: el pid %d aparece más de una vez", pids[k]);
        }
    }
    return true;
}
