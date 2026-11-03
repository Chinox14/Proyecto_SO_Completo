/*
 * memoria.c — Núcleo del simulador: iniciar, asignar, liberar e imprimir.
 *
 * Representación: m->bloques[0 .. n-1] es una lista ordenada por dirección.
 * Entre todos los bloques cubren la memoria completa, sin solaparse.
 *
 *   inicio:  0         12   17                 67   72         92    100
 *            | libre 12 | P2 | libre 50         | P4 | libre 20 | P6 |
 *
 * Como la lista está ordenada, los vecinos físicos de bloques[i] son
 * bloques[i-1] y bloques[i+1]. Eso hace sencillo fundir huecos al liberar.
 */
#include "memoria.h"

#include <stdio.h>
#include <string.h>

/* ---------- Funciones auxiliares (privadas: 'static') ---------- */

/* Redondea x hacia arriba al múltiplo de u. Ej.: (40, 16) -> 48 */
static int redondear_arriba(int x, int u)
{
    return ((x + u - 1) / u) * u;
}

/* Inserta 'b' en la posición 'pos' corriendo hacia la derecha los demás.
 * Precondición: hay espacio (m->n < MAX_BLOQUES). */
static void insertar_en(Memoria *m, int pos, Bloque b)
{
    memmove(&m->bloques[pos + 1], &m->bloques[pos],
            (size_t)(m->n - pos) * sizeof(Bloque));
    m->bloques[pos] = b;
    m->n++;
}

/* Quita el bloque de la posición 'pos' corriendo hacia la izquierda los demás. */
static void quitar_en(Memoria *m, int pos)
{
    memmove(&m->bloques[pos], &m->bloques[pos + 1],
            (size_t)(m->n - pos - 1) * sizeof(Bloque));
    m->n--;
}

/* Devuelve el índice del bloque ocupado por 'pid', o -1 si no existe. */
static int buscar_pid(const Memoria *m, int pid)
{
    for (int i = 0; i < m->n; i++) {
        if (!m->bloques[i].libre && m->bloques[i].pid == pid) {
            return i;
        }
    }
    return -1;
}

/* Suma de todos los huecos. Sirve para clasificar el tipo de rechazo. */
static int libre_total(const Memoria *m)
{
    int suma = 0;
    for (int i = 0; i < m->n; i++) {
        if (m->bloques[i].libre) {
            suma += m->bloques[i].tamano;
        }
    }
    return suma;
}

/* ---------- Operaciones públicas ---------- */

void memoria_iniciar(Memoria *m, int total, int unidad)
{
    /* Al inicio, toda la memoria es un único hueco (como en los apuntes). */
    m->total  = total;
    m->unidad = unidad;
    m->n      = 1;
    m->bloques[0] = (Bloque){
        .inicio = 0, .tamano = total, .libre = true, .pid = -1, .solicitado = 0
    };
}

Resultado memoria_asignar(Memoria *m, int pid, int solicitado,
                          FnEstrategia elegir, int *revisados)
{
    if (revisados != NULL) {
        *revisados = 0;
    }

    /* 1. Validar la petición */
    if (pid < 0 || solicitado <= 0) {
        return ERROR_PARAMETRO_INVALIDO;
    }
    if (buscar_pid(m, pid) >= 0) {
        return ERROR_PID_DUPLICADO;
    }

    /* 2. Redondear a la unidad: aquí nace la fragmentación INTERNA */
    int tam = redondear_arriba(solicitado, m->unidad);

    /* 3. La estrategia elige el hueco (es lo único que cambia entre las tres) */
    int rev = 0;
    int i = elegir(m, tam, &rev);
    if (revisados != NULL) {
        *revisados = rev;
    }

    /* 4. Ningún hueco sirve: ¿falta memoria o está partida? */
    if (i < 0) {
        return (libre_total(m) >= tam) ? RECHAZO_FRAGMENTACION  /* frag. EXTERNA */
                                       : RECHAZO_SIN_ESPACIO;
    }

    /* Defensa: no confiamos ciegamente en la estrategia */
    if (i >= m->n || !m->bloques[i].libre || m->bloques[i].tamano < tam) {
        return ERROR_ESTRATEGIA_INVALIDA;
    }

    /* 5. Dividir el hueco si sobra espacio:
     *      antes:   | libre 50            |
     *      después: | P7 18 | libre 32    |                                */
    int sobra = m->bloques[i].tamano - tam;
    if (sobra > 0) {
        if (m->n >= MAX_BLOQUES) {
            return ERROR_SIN_BLOQUES;   /* no se modificó nada todavía */
        }
        Bloque resto = {
            .inicio = m->bloques[i].inicio + tam, .tamano = sobra,
            .libre = true, .pid = -1, .solicitado = 0
        };
        insertar_en(m, i + 1, resto);
    }

    /* 6. El hueco (ya recortado) pasa a ser la partición del proceso */
    m->bloques[i].tamano     = tam;
    m->bloques[i].libre      = false;
    m->bloques[i].pid        = pid;
    m->bloques[i].solicitado = solicitado;
    return OK;
}

Resultado memoria_liberar(Memoria *m, int pid)
{
    int i = buscar_pid(m, pid);
    if (i < 0) {
        return ERROR_PID_INEXISTENTE;
    }

    /* 1. El bloque vuelve a ser hueco */
    m->bloques[i].libre      = true;
    m->bloques[i].pid        = -1;
    m->bloques[i].solicitado = 0;

    /* 2. Fundir con el vecino DERECHO si está libre.
     *    Se hace primero porque no cambia el índice i.                     */
    if (i + 1 < m->n && m->bloques[i + 1].libre) {
        m->bloques[i].tamano += m->bloques[i + 1].tamano;
        quitar_en(m, i + 1);
    }

    /* 3. Fundir con el vecino IZQUIERDO si está libre.
     *    El izquierdo absorbe al actual (su 'inicio' es el correcto).      */
    if (i > 0 && m->bloques[i - 1].libre) {
        m->bloques[i - 1].tamano += m->bloques[i].tamano;
        quitar_en(m, i);
    }
    return OK;
}

void memoria_imprimir(const Memoria *m)
{
    printf("|");
    for (int i = 0; i < m->n; i++) {
        const Bloque *b = &m->bloques[i];
        if (b->libre) {
            printf(" libre %d |", b->tamano);
        } else {
            printf(" P%d %d |", b->pid, b->tamano);
        }
    }
    printf("\n");
}

const char *resultado_texto(Resultado r)
{
    switch (r) {
    case OK:                        return "OK";
    case RECHAZO_SIN_ESPACIO:       return "RECHAZO_SIN_ESPACIO";
    case RECHAZO_FRAGMENTACION:     return "RECHAZO_FRAGMENTACION";
    case ERROR_PID_DUPLICADO:       return "ERROR_PID_DUPLICADO";
    case ERROR_PID_INEXISTENTE:     return "ERROR_PID_INEXISTENTE";
    case ERROR_SIN_BLOQUES:         return "ERROR_SIN_BLOQUES";
    case ERROR_PARAMETRO_INVALIDO:  return "ERROR_PARAMETRO_INVALIDO";
    case ERROR_ESTRATEGIA_INVALIDA: return "ERROR_ESTRATEGIA_INVALIDA";
    }
    return "DESCONOCIDO";
}
