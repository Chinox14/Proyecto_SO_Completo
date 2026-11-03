/*
 * test_metricas.c — Pruebas de metricas.c (Fase 5)
 *
 * Los valores esperados están calculados a mano en los comentarios,
 * para que cualquiera del equipo pueda revisarlos con lápiz y papel.
 */
#include "memoria.h"

#include <math.h>
#include <stdio.h>

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

/* Compara decimales con tolerancia (los double no son exactos) */
#define CERCA(a, b) (fabs((a) - (b)) < 1e-9)
#define CHECK_D(real, esperado, nombre) \
    CHECK(CERCA(real, esperado), "%s: esperaba %.6f, dio %.6f", nombre, (double)(esperado), (double)(real))

static Memoria m;

static void prueba_mapa_quiz(void)
{
    /* | libre 12 | P2 5 | libre 50 | P4 5 | libre 20 | P6 8 |   (total 100, unidad 1)
     *   ocupado = 5+5+8 = 18        libre = 12+50+20 = 82
     *   hueco mayor = 50            huecos = 3, ocupados = 3
     *   frag. interna = 0 (unidad 1, nada se redondea)
     *   utilización = 18/100 = 0,18
     *   frag. externa = 1 - 50/82 = 0,390243...
     *   relación de huecos = 3/3 = 1                                      */
    puts("T1 mapa del quiz (unidad 1)");
    memoria_iniciar(&m, 100, 1);
    int tam[] = {12, 5, 50, 5, 20, 8};
    for (int p = 1; p <= 6; p++) memoria_asignar(&m, p, tam[p - 1], elegir_first_fit, NULL);
    memoria_liberar(&m, 1);
    memoria_liberar(&m, 3);
    memoria_liberar(&m, 5);

    Metricas f = memoria_metricas(&m);
    CHECK(f.ocupado == 18 && f.libre == 82, "ocupado/libre: %d/%d", f.ocupado, f.libre);
    CHECK(f.hueco_mayor == 50, "hueco mayor: %d", f.hueco_mayor);
    CHECK(f.num_huecos == 3 && f.num_ocupados == 3, "huecos/ocupados: %d/%d", f.num_huecos, f.num_ocupados);
    CHECK(f.frag_interna == 0, "frag. interna: %d", f.frag_interna);
    CHECK_D(f.utilizacion, 0.18, "utilización");
    CHECK_D(f.frag_externa, 1.0 - 50.0 / 82.0, "frag. externa");
    CHECK_D(f.relacion_huecos, 1.0, "relación de huecos");
}

static void prueba_unidad_16(void)
{
    /* Memoria 256, unidad 16.  P1 pide 40 -> recibe 48.  P2 pide 20 -> recibe 32.
     * | P1 48 | P2 32 | libre 176 |
     *   ocupado = 80, frag. interna = 8 + 12 = 20
     *   utilización = (40+20)/256 = 0,234375   (lo pedido, no lo asignado)
     *   frag. externa = 1 - 176/176 = 0 (un solo hueco)
     *   relación de huecos = 1/2 = 0,5                                    */
    puts("T2 unidad 16: la fragmentación interna aparece");
    memoria_iniciar(&m, 256, 16);
    memoria_asignar(&m, 1, 40, elegir_first_fit, NULL);
    memoria_asignar(&m, 2, 20, elegir_first_fit, NULL);

    Metricas f = memoria_metricas(&m);
    CHECK(f.ocupado == 80 && f.libre == 176, "ocupado/libre: %d/%d", f.ocupado, f.libre);
    CHECK(f.frag_interna == 20, "frag. interna: esperaba 20, dio %d", f.frag_interna);
    CHECK_D(f.utilizacion, 60.0 / 256.0, "utilización");
    CHECK_D(f.frag_externa, 0.0, "frag. externa");
    CHECK_D(f.relacion_huecos, 0.5, "relación de huecos");
}

static void prueba_bordes(void)
{
    puts("T3 bordes: memoria vacía y memoria llena (sin dividir por 0)");
    memoria_iniciar(&m, 256, 1);
    Metricas f = memoria_metricas(&m);
    CHECK(f.libre == 256 && f.num_huecos == 1 && f.num_ocupados == 0, "memoria vacía mal medida");
    CHECK_D(f.utilizacion, 0.0, "utilización vacía");
    CHECK_D(f.frag_externa, 0.0, "frag. externa vacía (un solo hueco)");
    CHECK_D(f.relacion_huecos, 0.0, "relación sin ocupados");

    memoria_asignar(&m, 1, 256, elegir_first_fit, NULL);
    f = memoria_metricas(&m);
    CHECK(f.libre == 0 && f.num_huecos == 0, "memoria llena mal medida");
    CHECK_D(f.frag_externa, 0.0, "frag. externa con 0 libre");
    CHECK_D(f.utilizacion, 1.0, "utilización llena");
    CHECK(!isnan(f.frag_externa) && !isnan(f.relacion_huecos), "apareció NaN");
}

static void prueba_contadores(void)
{
    /* 3 asignaciones OK (revisan 1, 3 y 5), 1 rechazo por fragmentación (revisa 7),
     * 1 rechazo sin espacio (revisa 7), 1 error de pid duplicado, 2 liberaciones
     * (1 OK y 1 con error).
     *   tasa de rechazo = 2 / 5 válidas = 0,4  (el error NO cuenta)
     *   promedio éxito  = (1+3+5)/3 = 3
     *   promedio rechazo = (7+7)/2 = 7                                    */
    puts("T4 contadores y coste de búsqueda separado");
    Estadisticas e;
    estadisticas_iniciar(&e);
    estadisticas_registrar_asignacion(&e, OK, 1);
    estadisticas_registrar_asignacion(&e, OK, 3);
    estadisticas_registrar_asignacion(&e, OK, 5);
    estadisticas_registrar_asignacion(&e, RECHAZO_FRAGMENTACION, 7);
    estadisticas_registrar_asignacion(&e, RECHAZO_SIN_ESPACIO, 7);
    /* Un error con revisados > 0 (una estrategia defectuosa sí alcanza a buscar):
     * NO debe sumarse al coste. Con 0 esta prueba no detectaría el error. */
    estadisticas_registrar_asignacion(&e, ERROR_ESTRATEGIA_INVALIDA, 4);
    estadisticas_registrar_liberacion(&e, OK);
    estadisticas_registrar_liberacion(&e, ERROR_PID_INEXISTENTE);

    CHECK(e.solicitudes == 6 && e.asignadas == 3, "solicitudes/asignadas: %d/%d", e.solicitudes, e.asignadas);
    CHECK(e.rechazos_fragmentacion == 1 && e.rechazos_sin_espacio == 1, "rechazos mal contados");
    CHECK(e.errores == 2 && e.liberaciones == 1, "errores/liberaciones: %d/%d", e.errores, e.liberaciones);
    CHECK_D(estadisticas_tasa_rechazo(&e), 0.4, "tasa de rechazo");
    CHECK_D(estadisticas_rev_prom_exito(&e), 3.0, "revisados por éxito");
    CHECK_D(estadisticas_rev_prom_rechazo(&e), 7.0, "revisados por rechazo");

    Estadisticas vacia;
    estadisticas_iniciar(&vacia);
    CHECK_D(estadisticas_tasa_rechazo(&vacia), 0.0, "tasa sin datos");
    CHECK_D(estadisticas_rev_prom_exito(&vacia), 0.0, "promedio sin datos");
    CHECK_D(estadisticas_relacion_huecos_prom(&vacia), 0.0, "relación sin datos");
}

static void prueba_promedios(void)
{
    /* Tres fotos:
     *   A: util 0,2  ext 0,0  rel 0 (sin ocupados -> NO cuenta en la relación)  int 0   huecos 1
     *   B: util 0,4  ext 0,5  rel 1,0                                            int 6   huecos 3
     *   C: util 0,6  ext 0,2  rel 0,5                                            int 12  huecos 2
     *   utilización prom = 1,2/3 = 0,4
     *   frag. externa prom = 0,7/3 = 0,2333...;  máxima = 0,5
     *   relación prom = (1,0 + 0,5) / 2 = 0,75  (solo B y C)
     *   frag. interna prom = 18/3 = 6;  máximo de huecos = 3               */
    puts("T5 promedios en el tiempo y picos");
    Estadisticas e;
    estadisticas_iniciar(&e);
    Metricas a = {.utilizacion = 0.2, .frag_externa = 0.0, .relacion_huecos = 0.0,
                  .num_ocupados = 0, .frag_interna = 0, .num_huecos = 1};
    Metricas b = {.utilizacion = 0.4, .frag_externa = 0.5, .relacion_huecos = 1.0,
                  .num_ocupados = 3, .frag_interna = 6, .num_huecos = 3};
    Metricas c = {.utilizacion = 0.6, .frag_externa = 0.2, .relacion_huecos = 0.5,
                  .num_ocupados = 4, .frag_interna = 12, .num_huecos = 2};
    estadisticas_muestrear(&e, &a);
    estadisticas_muestrear(&e, &b);
    estadisticas_muestrear(&e, &c);

    CHECK_D(estadisticas_utilizacion_prom(&e), 0.4, "utilización promedio");
    CHECK_D(estadisticas_frag_externa_prom(&e), 0.7 / 3.0, "frag. externa promedio");
    CHECK_D(e.max_frag_externa, 0.5, "frag. externa máxima");
    CHECK_D(estadisticas_relacion_huecos_prom(&e), 0.75, "relación de huecos promedio");
    CHECK_D(estadisticas_frag_interna_prom(&e), 6.0, "frag. interna promedio");
    CHECK(e.max_huecos == 3, "máximo de huecos: %d", e.max_huecos);
}

static void prueba_coherencia(void)
{
    /* En 20 000 operaciones aleatorias, después de cada una:
     *   ocupado + libre == total, frag. interna == ocupado - solicitado,
     *   0 <= métricas <= 1, hueco mayor <= libre.                         */
    puts("T6 coherencia de las métricas en 20 000 operaciones (unidad 4)");
    enum { PIDS = 60 };
    int vivo[PIDS] = {0};
    unsigned int x = 7;  /* generador propio y simple: no depende de rand() */
    memoria_iniciar(&m, 256, 4);
    int fallos_antes = fallos;
    for (int op = 0; op < 20000 && fallos == fallos_antes; op++) {
        x = x * 1103515245u + 12345u;
        int pid = (int)((x >> 8) % PIDS);
        int t = 1 + (int)((x >> 16) % 30);
        if (!vivo[pid]) {
            if (memoria_asignar(&m, pid, t, elegir_best_fit, NULL) == OK) vivo[pid] = t;
        } else {
            memoria_liberar(&m, pid);
            vivo[pid] = 0;
        }
        Metricas f = memoria_metricas(&m);
        int pedido = 0;
        for (int p = 0; p < PIDS; p++) pedido += vivo[p];
        CHECK(f.ocupado + f.libre == m.total, "op %d: ocupado+libre != total", op);
        CHECK(f.frag_interna == f.ocupado - pedido, "op %d: frag. interna incoherente", op);
        CHECK(f.hueco_mayor <= f.libre, "op %d: hueco mayor > libre", op);
        CHECK(f.utilizacion >= 0 && f.utilizacion <= 1, "op %d: utilización fuera de [0,1]", op);
        CHECK(f.frag_externa >= 0 && f.frag_externa < 1, "op %d: frag. externa fuera de [0,1)", op);
    }
}

int main(void)
{
    prueba_mapa_quiz();
    prueba_unidad_16();
    prueba_bordes();
    prueba_contadores();
    prueba_promedios();
    prueba_coherencia();

    printf("\n%d comprobaciones, %d fallos\n", pruebas, fallos);
    return fallos == 0 ? 0 : 1;
}
