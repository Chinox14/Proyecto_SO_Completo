/*
 * main.c — Programa principal del simulador (Fase 6)
 *
 * Uso:
 *   ./simulador [opciones]
 *
 * Entrada (una de las dos):
 *   --archivo RUTA        secuencia en archivo (A <pid> <KB> / L <pid>)
 *   --semilla N           secuencia aleatoria reproducible (por defecto 1)
 *       --eventos N         número de eventos            (por defecto 5000)
 *       --tmin N --tmax N   tamaños de proceso en KB     (por defecto 4 y 40)
 *       --vivos N           procesos vivos objetivo      (por defecto 9)
 *
 * Configuración:
 *   --memoria KB          memoria total                (por defecto 256)
 *   --unidad KB           unidad de asignación         (por defecto 1)
 *   --estrategia E        first | best | worst | todas (por defecto todas)
 *
 * Salida:
 *   (siempre)             tabla resumen por estrategia
 *   --traza               cada evento con el mapa de memoria (para secuencias cortas)
 *   --csv RUTA            agrega una fila de resultados por estrategia (crea el encabezado)
 *   --serie RUTA          métricas después de CADA evento (para gráficos en el tiempo)
 *   --escenario NOMBRE    etiqueta que se escribe en el CSV
 *
 * Después de cada operación se verifican los invariantes I1–I7.
 * Si alguno falla, el programa se detiene (código de salida 2).
 */
#include "memoria.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char  *NOMBRES[]  = {"first", "best", "worst"};
static const char  *TITULOS[]  = {"Primer ajuste", "Mejor ajuste", "Peor ajuste"};
static const FnEstrategia FUNCIONES[] = {elegir_first_fit, elegir_best_fit, elegir_worst_fit};

/* Estructuras grandes: 'static' para no cargar la pila */
static Secuencia sec;
static Memoria   mem;
static bool      rechazado[MAX_EVENTOS];

typedef struct {
    const char *archivo;
    unsigned int semilla;
    int eventos, tmin, tmax, vivos;
    int memoria, unidad;
    int estrategia;           /* 0, 1, 2 o -1 = todas */
    bool traza;
    const char *csv, *serie, *escenario;
} Opciones;

static void uso(void)
{
    fprintf(stderr,
        "Uso: ./simulador [--archivo RUTA | --semilla N --eventos N --tmin N --tmax N --vivos N]\n"
        "                 [--memoria KB] [--unidad KB] [--estrategia first|best|worst|todas]\n"
        "                 [--traza] [--csv RUTA] [--serie RUTA] [--escenario NOMBRE]\n");
    exit(1);
}

/* Lee un entero positivo o termina con un mensaje claro */
static int entero(const char *texto, const char *opcion)
{
    char *fin;
    long v = strtol(texto, &fin, 10);
    if (*fin != '\0' || v <= 0 || v > 1000000000L) {
        fprintf(stderr, "Valor no válido para %s: '%s' (debe ser un entero > 0)\n", opcion, texto);
        exit(1);
    }
    return (int)v;
}

static Opciones leer_opciones(int argc, char **argv)
{
    Opciones o = {
        .archivo = NULL, .semilla = 1, .eventos = 5000, .tmin = 4, .tmax = 40, .vivos = 9,
        .memoria = MEMORIA_DEFECTO, .unidad = UNIDAD_DEFECTO, .estrategia = -1,
        .traza = false, .csv = NULL, .serie = NULL, .escenario = "manual"
    };
    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];
        bool tiene_valor = (i + 1 < argc);
        #define VALOR() (tiene_valor ? argv[++i] : (uso(), ""))
        if      (strcmp(a, "--archivo") == 0)    o.archivo = VALOR();
        else if (strcmp(a, "--semilla") == 0)    o.semilla = (unsigned int)entero(VALOR(), a);
        else if (strcmp(a, "--eventos") == 0)    o.eventos = entero(VALOR(), a);
        else if (strcmp(a, "--tmin") == 0)       o.tmin = entero(VALOR(), a);
        else if (strcmp(a, "--tmax") == 0)       o.tmax = entero(VALOR(), a);
        else if (strcmp(a, "--vivos") == 0)      o.vivos = entero(VALOR(), a);
        else if (strcmp(a, "--memoria") == 0)    o.memoria = entero(VALOR(), a);
        else if (strcmp(a, "--unidad") == 0)     o.unidad = entero(VALOR(), a);
        else if (strcmp(a, "--csv") == 0)        o.csv = VALOR();
        else if (strcmp(a, "--serie") == 0)      o.serie = VALOR();
        else if (strcmp(a, "--escenario") == 0)  o.escenario = VALOR();
        else if (strcmp(a, "--traza") == 0)      o.traza = true;
        else if (strcmp(a, "--estrategia") == 0) {
            const char *e = VALOR();
            if      (strcmp(e, "first") == 0) o.estrategia = 0;
            else if (strcmp(e, "best") == 0)  o.estrategia = 1;
            else if (strcmp(e, "worst") == 0) o.estrategia = 2;
            else if (strcmp(e, "todas") == 0) o.estrategia = -1;
            else uso();
        }
        else uso();
        #undef VALOR
    }
    if (o.memoria % o.unidad != 0) {
        fprintf(stderr, "La memoria (%d) debe ser múltiplo de la unidad (%d)\n", o.memoria, o.unidad);
        exit(1);
    }
    if (o.tmin > o.tmax) {
        fprintf(stderr, "--tmin no puede ser mayor que --tmax\n");
        exit(1);
    }
    return o;
}

/* Detiene el programa si la memoria quedó inconsistente */
static void verificar_o_abortar(int evento)
{
    char motivo[200];
    if (!memoria_verificar(&mem, motivo, sizeof motivo)) {
        fprintf(stderr, "INVARIANTE ROTO después del evento %d: %s\n", evento, motivo);
        memoria_imprimir(&mem);
        exit(2);
    }
}

/* Ejecuta la secuencia completa con UNA estrategia */
static Estadisticas ejecutar(const Opciones *o, int e, FILE *serie)
{
    Estadisticas est;
    estadisticas_iniciar(&est);
    memoria_iniciar(&mem, o->memoria, o->unidad);
    memset(rechazado, 0, sizeof rechazado);

    if (o->traza) {
        printf("\n=== %s (memoria %d KB, unidad %d KB) ===\n", TITULOS[e], o->memoria, o->unidad);
        printf("inicio: ");
        memoria_imprimir(&mem);
    }

    for (int k = 0; k < sec.n; k++) {
        const Evento *ev = &sec.eventos[k];
        Resultado r;
        int rev = 0;

        if (ev->tipo == EV_ASIGNAR) {
            r = memoria_asignar(&mem, ev->pid, ev->tamano, FUNCIONES[e], &rev);
            estadisticas_registrar_asignacion(&est, r, rev);
            if (r == RECHAZO_FRAGMENTACION || r == RECHAZO_SIN_ESPACIO) {
                rechazado[ev->pid] = true;
            }
            if (o->traza) {
                printf("A P%-4d %4d KB -> %-22s ", ev->pid, ev->tamano, resultado_texto(r));
            }
        } else {
            if (rechazado[ev->pid]) {
                /* Ese proceso nunca entró a memoria: su fin no se cuenta */
                rechazado[ev->pid] = false;
                if (o->traza) printf("L P%-4d          -> (omitida: el proceso fue rechazado)\n", ev->pid);
                continue;
            }
            r = memoria_liberar(&mem, ev->pid);
            estadisticas_registrar_liberacion(&est, r);
            if (o->traza) {
                printf("L P%-4d          -> %-22s ", ev->pid, resultado_texto(r));
            }
        }

        verificar_o_abortar(k);
        Metricas f = memoria_metricas(&mem);
        estadisticas_muestrear(&est, &f);

        if (o->traza) memoria_imprimir(&mem);
        if (serie != NULL) {
            fprintf(serie, "%s,%s,%u,%d,%d,%.6f,%.6f,%d,%d,%d,%.6f\n",
                    o->escenario, NOMBRES[e], o->semilla, o->unidad, k,
                    f.utilizacion, f.frag_externa, f.frag_interna,
                    f.num_huecos, f.num_ocupados, f.relacion_huecos);
        }
    }
    return est;
}

static void imprimir_tabla(const Opciones *o, const Estadisticas *res, const int *cuales, int k)
{
    printf("\nEscenario: %s | memoria %d KB | unidad %d KB | %d eventos",
           o->escenario, o->memoria, o->unidad, sec.n);
    if (o->archivo) printf(" | archivo %s\n", o->archivo);
    else printf(" | semilla %u, tamaños %d-%d KB, vivos objetivo %d\n",
                o->semilla, o->tmin, o->tmax, o->vivos);

    printf("%-14s %9s %9s %9s %9s %9s %9s %9s %9s %9s\n",
           "Estrategia", "Asignadas", "Rech.frag", "Rech.esp", "%Rechazo",
           "Util.prom", "FExt.prom", "FInt.prom", "Rel.huec", "Rev.exito");
    for (int j = 0; j < k; j++) {
        const Estadisticas *s = &res[j];
        printf("%-14s %9d %9d %9d %8.2f%% %8.2f%% %8.3f %7.2fKB %9.3f %9.2f\n",
               TITULOS[cuales[j]], s->asignadas, s->rechazos_fragmentacion,
               s->rechazos_sin_espacio, 100 * estadisticas_tasa_rechazo(s),
               100 * estadisticas_utilizacion_prom(s), estadisticas_frag_externa_prom(s),
               estadisticas_frag_interna_prom(s), estadisticas_relacion_huecos_prom(s),
               estadisticas_rev_prom_exito(s));
    }
}

static void escribir_csv(const Opciones *o, const Estadisticas *res, const int *cuales, int k)
{
    FILE *prueba = fopen(o->csv, "r");
    bool nuevo = (prueba == NULL);
    if (prueba) fclose(prueba);

    FILE *f = fopen(o->csv, "a");
    if (f == NULL) {
        fprintf(stderr, "No se pudo escribir '%s'\n", o->csv);
        exit(1);
    }
    if (nuevo) {
        fprintf(f, "escenario,estrategia,semilla,memoria,unidad,eventos,vivos_objetivo,tmin,tmax,"
                   "solicitudes,asignadas,rechazos_frag,rechazos_espacio,errores,tasa_rechazo,"
                   "utilizacion_prom,frag_externa_prom,frag_externa_max,frag_interna_prom_kb,"
                   "relacion_huecos_prom,max_huecos,revisados_exito_prom,revisados_rechazo_prom\n");
    }
    for (int j = 0; j < k; j++) {
        const Estadisticas *s = &res[j];
        fprintf(f, "%s,%s,%u,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%.6f,%.6f,%.6f,%.6f,%.4f,%.6f,%d,%.4f,%.4f\n",
                o->escenario, NOMBRES[cuales[j]], o->archivo ? 0u : o->semilla,
                o->memoria, o->unidad, sec.n,
                o->archivo ? 0 : o->vivos, o->archivo ? 0 : o->tmin, o->archivo ? 0 : o->tmax,
                s->solicitudes, s->asignadas, s->rechazos_fragmentacion, s->rechazos_sin_espacio,
                s->errores, estadisticas_tasa_rechazo(s), estadisticas_utilizacion_prom(s),
                estadisticas_frag_externa_prom(s), s->max_frag_externa,
                estadisticas_frag_interna_prom(s), estadisticas_relacion_huecos_prom(s),
                s->max_huecos, estadisticas_rev_prom_exito(s), estadisticas_rev_prom_rechazo(s));
    }
    fclose(f);
}

int main(int argc, char **argv)
{
    Opciones o = leer_opciones(argc, argv);

    if (o.archivo != NULL) {
        if (!secuencia_cargar(&sec, o.archivo)) return 1;
    } else {
        secuencia_generar(&sec, o.semilla, o.eventos, o.tmin, o.tmax, o.vivos);
    }

    FILE *serie = NULL;
    if (o.serie != NULL) {
        FILE *prueba = fopen(o.serie, "r");
        bool nuevo = (prueba == NULL);
        if (prueba) fclose(prueba);
        serie = fopen(o.serie, "a");
        if (serie == NULL) { fprintf(stderr, "No se pudo escribir '%s'\n", o.serie); return 1; }
        if (nuevo) fprintf(serie, "escenario,estrategia,semilla,unidad,evento,utilizacion,"
                                  "frag_externa,frag_interna_kb,huecos,ocupados,relacion_huecos\n");
    }

    Estadisticas res[3];
    int cuales[3];
    int k = 0;
    for (int e = 0; e < 3; e++) {
        if (o.estrategia != -1 && o.estrategia != e) continue;
        res[k] = ejecutar(&o, e, serie);   /* MISMA secuencia 'sec' para todas */
        cuales[k++] = e;
    }

    if (serie) fclose(serie);
    imprimir_tabla(&o, res, cuales, k);
    if (o.csv != NULL) escribir_csv(&o, res, cuales, k);
    return 0;
}
