/*
 * metricas.c — Métricas de un instante y estadísticas de una ejecución (Fase 5)
 *
 * Definiciones (las mismas que van en el informe):
 *
 *   Utilización          = KB solicitados por los procesos / KB totales
 *                          (lo que realmente se usa; no cuenta el redondeo)
 *   Fragmentación interna = suma de (tamaño asignado - tamaño solicitado)
 *                          (desperdicio DENTRO de las particiones)
 *   Fragmentación externa = 1 - hueco mayor / memoria libre total
 *                          0 -> todo lo libre está en un solo hueco
 *                          cerca de 1 -> lo libre está muy repartido
 *   Relación de huecos   = huecos / bloques ocupados
 *                          (regla del 50 %: con primer ajuste tiende a ~0,5)
 */
#include "memoria.h"

/* División segura: si no hay denominador, la métrica vale 0 */
static double dividir(double a, double b)
{
    return (b != 0.0) ? a / b : 0.0;
}

/* ---------- Foto de un instante ---------- */

Metricas memoria_metricas(const Memoria *m)
{
    Metricas f = {0};
    int solicitado_util = 0;

    for (int i = 0; i < m->n; i++) {
        const Bloque *b = &m->bloques[i];
        if (b->libre) {
            f.libre += b->tamano;
            f.num_huecos++;
            if (b->tamano > f.hueco_mayor) {
                f.hueco_mayor = b->tamano;
            }
        } else {
            f.ocupado += b->tamano;
            f.num_ocupados++;
            f.frag_interna += b->tamano - b->solicitado;
            solicitado_util += b->solicitado;
        }
    }

    f.utilizacion     = dividir(solicitado_util, m->total);
    f.frag_externa    = (f.libre > 0) ? 1.0 - dividir(f.hueco_mayor, f.libre) : 0.0;
    f.relacion_huecos = dividir(f.num_huecos, f.num_ocupados);
    return f;
}

/* ---------- Estadísticas de una ejecución ---------- */

void estadisticas_iniciar(Estadisticas *e)
{
    *e = (Estadisticas){0};
}

void estadisticas_registrar_asignacion(Estadisticas *e, Resultado r, int revisados)
{
    e->solicitudes++;
    switch (r) {
    case OK:
        e->asignadas++;
        e->revisados_exito += revisados;
        break;
    case RECHAZO_SIN_ESPACIO:
        e->rechazos_sin_espacio++;
        e->revisados_rechazo += revisados;
        break;
    case RECHAZO_FRAGMENTACION:
        e->rechazos_fragmentacion++;
        e->revisados_rechazo += revisados;
        break;
    default:
        /* Errores de entrada (pid duplicado, etc.): no son una búsqueda
         * real de la estrategia, así que no cuentan en el coste. */
        e->errores++;
        break;
    }
}

void estadisticas_registrar_liberacion(Estadisticas *e, Resultado r)
{
    if (r == OK) {
        e->liberaciones++;
    } else {
        e->errores++;
    }
}

void estadisticas_muestrear(Estadisticas *e, const Metricas *f)
{
    e->muestras++;
    e->suma_utilizacion  += f->utilizacion;
    e->suma_frag_externa += f->frag_externa;
    e->suma_frag_interna += f->frag_interna;

    /* La relación de huecos no tiene sentido con 0 procesos: se omite */
    if (f->num_ocupados > 0) {
        e->suma_relacion_huecos += f->relacion_huecos;
        e->muestras_relacion++;
    }

    if (f->frag_externa > e->max_frag_externa) {
        e->max_frag_externa = f->frag_externa;
    }
    if (f->num_huecos > e->max_huecos) {
        e->max_huecos = f->num_huecos;
    }
}

/* ---------- Valores derivados ---------- */

double estadisticas_tasa_rechazo(const Estadisticas *e)
{
    /* Solo cuentan las solicitudes válidas: los errores de entrada no son
     * culpa de la estrategia. */
    int validas = e->asignadas + e->rechazos_sin_espacio + e->rechazos_fragmentacion;
    return dividir(e->rechazos_sin_espacio + e->rechazos_fragmentacion, validas);
}

double estadisticas_rev_prom_exito(const Estadisticas *e)
{
    return dividir((double)e->revisados_exito, e->asignadas);
}

double estadisticas_rev_prom_rechazo(const Estadisticas *e)
{
    return dividir((double)e->revisados_rechazo,
                   e->rechazos_sin_espacio + e->rechazos_fragmentacion);
}

double estadisticas_utilizacion_prom(const Estadisticas *e)
{
    return dividir(e->suma_utilizacion, e->muestras);
}

double estadisticas_frag_externa_prom(const Estadisticas *e)
{
    return dividir(e->suma_frag_externa, e->muestras);
}

double estadisticas_relacion_huecos_prom(const Estadisticas *e)
{
    return dividir(e->suma_relacion_huecos, e->muestras_relacion);
}

double estadisticas_frag_interna_prom(const Estadisticas *e)
{
    return dividir(e->suma_frag_interna, e->muestras);
}
