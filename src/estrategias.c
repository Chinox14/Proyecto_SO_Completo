/*
 * estrategias.c — Primer, mejor y peor ajuste (Fase 4)
 *
 * Las tres funciones solo ELIGEN un hueco; no modifican la memoria.
 * Devuelven el índice del hueco elegido en m->bloques, o -1 si ninguno sirve.
 * En '*revisados' dejan cuántos bloques examinaron (coste de búsqueda).
 *
 * Regla de desempate en mejor y peor ajuste: si dos huecos tienen el mismo
 * tamaño, gana el de MENOR dirección (el primero que aparece en la lista).
 * Se usa '<' / '>' estricto, no '<=' / '>=', para que eso se cumpla.
 */
#include "memoria.h"

/*
 * PRIMER AJUSTE (first fit)
 * Recorre la lista desde el inicio y se detiene en el PRIMER hueco que sirve.
 * Coste: en el mejor caso revisa 1 bloque; en el peor, todos.
 */
int elegir_first_fit(const Memoria *m, int tamano_redondeado, int *revisados)
{
    *revisados = 0;
    for (int i = 0; i < m->n; i++) {
        (*revisados)++;
        if (m->bloques[i].libre && m->bloques[i].tamano >= tamano_redondeado) {
            return i;   /* se detiene aquí: por eso suele ser más rápido */
        }
    }
    return -1;
}

/*
 * MEJOR AJUSTE (best fit)
 * Elige el hueco MÁS PEQUEÑO que sirve: deja el sobrante más chico posible.
 * Debe recorrer TODA la lista, porque un hueco mejor puede estar al final.
 * Atajo: si encuentra un hueco exacto (sobrante 0), no puede haber uno mejor
 * y se detiene.
 */
int elegir_best_fit(const Memoria *m, int tamano_redondeado, int *revisados)
{
    *revisados = 0;
    int elegido = -1;
    for (int i = 0; i < m->n; i++) {
        (*revisados)++;
        const Bloque *b = &m->bloques[i];
        if (!b->libre || b->tamano < tamano_redondeado) {
            continue;
        }
        if (elegido == -1 || b->tamano < m->bloques[elegido].tamano) {
            elegido = i;
            if (b->tamano == tamano_redondeado) {
                break;  /* ajuste exacto: imposible mejorarlo */
            }
        }
    }
    return elegido;
}

/*
 * PEOR AJUSTE (worst fit)
 * Elige el hueco MÁS GRANDE: la idea es que el sobrante quede grande y
 * todavía sirva para otro proceso. También recorre TODA la lista.
 */
int elegir_worst_fit(const Memoria *m, int tamano_redondeado, int *revisados)
{
    *revisados = 0;
    int elegido = -1;
    for (int i = 0; i < m->n; i++) {
        (*revisados)++;
        const Bloque *b = &m->bloques[i];
        if (!b->libre || b->tamano < tamano_redondeado) {
            continue;
        }
        if (elegido == -1 || b->tamano > m->bloques[elegido].tamano) {
            elegido = i;
        }
    }
    return elegido;
}
