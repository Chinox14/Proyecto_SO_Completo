/*
 * memoria.h — Contrato del simulador de gestión de memoria (Fase 2: diseño)
 *
 * Proyecto final de Sistemas Operativos — Opción 2
 * Particionado dinámico (asignación contigua) con tres estrategias:
 *   primer ajuste (first fit), mejor ajuste (best fit) y peor ajuste (worst fit).
 *
 * Decisiones de diseño (ver explicación en el informe):
 *   - Sin malloc: todo vive en arreglos de tamaño fijo, así no hay fugas.
 *   - La memoria es una LISTA DE BLOQUES ORDENADA POR DIRECCIÓN
 *     (la "tabla de huecos" de los apuntes, ampliada con los bloques ocupados).
 *     Cada bloque es un hueco (libre) o una partición ocupada por un proceso.
 *   - Las tres estrategias comparten el mismo código de asignación;
 *     lo único que cambia es la función que ELIGE el hueco.
 *     Eso garantiza una comparación justa.
 *   - Todas las cantidades son enteros en KB.
 *
 * Simplificación declarada (va en "limitaciones" del informe):
 *   si ningún hueco sirve, la solicitud se RECHAZA y se cuenta. Un SO real
 *   podría hacer esperar al proceso o atender primero a otro más pequeño
 *   de la cola de entrada.
 */
#ifndef MEMORIA_H
#define MEMORIA_H

#include <stdbool.h>

/* ---------- Límites fijos (sin memoria dinámica) ---------- */

#define MAX_PROCESOS   512   /* procesos vivos al mismo tiempo             */
/* Con P procesos puede haber un hueco antes, entre y después de cada uno:
 * P particiones + (P + 1) huecos = 2P + 1 bloques en el peor caso.      */
#define MAX_BLOQUES   (2 * MAX_PROCESOS + 1)
#define MAX_EVENTOS  10000   /* solicitudes en una secuencia               */

/* Valores por defecto (se pueden cambiar con argumentos al ejecutar) */
#define MEMORIA_DEFECTO  256  /* KB de memoria total simulada              */
#define UNIDAD_DEFECTO     1  /* KB; en los experimentos: 1, 4 y 16        */

/* ---------- Bloque de memoria ---------- */

typedef struct {
    int  inicio;      /* dirección inicial en KB                          */
    int  tamano;      /* tamaño real del bloque (múltiplo de la unidad)   */
    bool libre;       /* true = hueco, false = ocupado                    */
    int  pid;         /* dueño del bloque; -1 si está libre               */
    int  solicitado;  /* lo que pidió el proceso; 0 si está libre.
                         tamano - solicitado = fragmentación interna      */
} Bloque;

/* ---------- Memoria simulada ---------- */

typedef struct {
    int    total;                  /* tamaño total en KB                  */
    int    unidad;                 /* unidad de asignación en KB          */
    Bloque bloques[MAX_BLOQUES];   /* ordenados por 'inicio', sin huecos  */
    int    n;                      /* cuántos bloques hay en uso          */
} Memoria;

/* ---------- Estrategias ---------- */

typedef enum {
    FIRST_FIT = 0,   /* primer ajuste */
    BEST_FIT  = 1,   /* mejor ajuste  */
    WORST_FIT = 2    /* peor ajuste   */
} Estrategia;

/*
 * Una estrategia solo ELIGE: recibe la memoria y el tamaño ya redondeado
 * a la unidad, y devuelve el índice del hueco elegido o -1 si ninguno sirve.
 * No modifica la memoria.
 *
 * En '*revisados' deja cuántos bloques examinó antes de decidir.
 * Con eso medimos el COSTE DE BÚSQUEDA de cada estrategia y comprobamos
 * la afirmación de los apuntes: "el primer ajuste suele ser más rápido",
 * porque se detiene en el primer hueco que sirve, mientras que mejor y
 * peor ajuste deben recorrer toda la lista.
 */
typedef int (*FnEstrategia)(const Memoria *m, int tamano_redondeado, int *revisados);

int elegir_first_fit(const Memoria *m, int tamano_redondeado, int *revisados);
int elegir_best_fit (const Memoria *m, int tamano_redondeado, int *revisados);
int elegir_worst_fit(const Memoria *m, int tamano_redondeado, int *revisados);

/* ---------- Resultado de una operación ---------- */

typedef enum {
    OK = 0,
    RECHAZO_SIN_ESPACIO,     /* la memoria libre total no alcanza          */
    RECHAZO_FRAGMENTACION,   /* el libre total sí alcanza, pero ningún
                                hueco individual es suficiente (frag. ext.)*/
    ERROR_PID_DUPLICADO,     /* ese pid ya tiene memoria asignada          */
    ERROR_PID_INEXISTENTE,   /* se intenta liberar un pid que no existe    */
    ERROR_SIN_BLOQUES,       /* se llenó el arreglo de bloques             */
    ERROR_PARAMETRO_INVALIDO,/* pid < 0 o tamaño solicitado <= 0           */
    ERROR_ESTRATEGIA_INVALIDA/* la estrategia devolvió un índice que no es
                                un hueco suficiente (protege de bugs)      */
} Resultado;

/* Texto legible de un Resultado (para logs y mensajes) */
const char *resultado_texto(Resultado r);

/* ---------- Operaciones sobre la memoria ---------- */

/* Deja la memoria como un único hueco de 'total' KB. */
void memoria_iniciar(Memoria *m, int total, int unidad);

/*
 * Asigna memoria a 'pid' usando la estrategia dada:
 *   1. Redondea 'solicitado' hacia arriba al múltiplo de la unidad.
 *   2. Pide a la estrategia un hueco.
 *   3. Si sobra espacio, divide el hueco (partición + hueco sobrante).
 * En '*revisados' devuelve lo que informó la estrategia (puede ser NULL).
 */
Resultado memoria_asignar(Memoria *m, int pid, int solicitado,
                          FnEstrategia elegir, int *revisados);

/* Libera la memoria de 'pid' y funde el hueco con sus vecinos libres. */
Resultado memoria_liberar(Memoria *m, int pid);

/* Imprime el mapa: | P1 12 | libre 5 | ... */
void memoria_imprimir(const Memoria *m);

/* ---------- Invariantes (se verifican después de CADA operación) ---------- */

/*
 * Devuelve true si la memoria es consistente:
 *   I1. Los bloques son contiguos: inicio[i+1] == inicio[i] + tamano[i].
 *   I2. El primer bloque empieza en 0 y la suma de tamaños es 'total'.
 *   I3. Ningún tamaño es <= 0 y todos son múltiplos de la unidad.
 *   I4. No hay dos huecos libres seguidos (los adyacentes siempre se fundieron).
 *   I5. Cada pid aparece como máximo una vez.
 *   I6. En un bloque ocupado: 0 < solicitado <= tamano y tamano - solicitado < unidad.
 *   I7. Un hueco tiene pid == -1 y solicitado == 0; un ocupado tiene pid >= 0.
 * Si algo falla, escribe en 'motivo' cuál invariante se rompió.
 */
bool memoria_verificar(const Memoria *m, char *motivo, int tam_motivo);

/* ---------- Métricas de un instante (foto de la memoria) ---------- */

typedef struct {
    int    ocupado;            /* KB en bloques ocupados                    */
    int    libre;              /* KB en huecos                              */
    int    hueco_mayor;        /* KB del hueco más grande                   */
    int    num_huecos;         /* cuántos huecos hay                        */
    int    num_ocupados;       /* cuántos bloques ocupados hay              */
    int    frag_interna;       /* suma de (tamano - solicitado)             */
    double utilizacion;        /* solicitado útil / total, de 0 a 1         */
    double frag_externa;       /* 1 - hueco_mayor / libre (0 si libre == 0) */
    double relacion_huecos;    /* num_huecos / num_ocupados (0 si no hay
                                  ocupados). Regla del 50 % de los apuntes:
                                  con primer ajuste tiende a ~0,5          */
} Metricas;

Metricas memoria_metricas(const Memoria *m);

/* ---------- Estadísticas acumuladas de una ejecución completa ---------- */

/*
 * Una "foto" (Metricas) dice cómo está la memoria en UN instante.
 * Las Estadisticas resumen TODA la ejecución:
 *   - contadores de lo que pasó con cada solicitud;
 *   - coste de búsqueda SEPARADO en éxitos y rechazos (lección de la Fase 4:
 *     mezclarlos escondía que el primer ajuste es más rápido);
 *   - promedios en el tiempo: se toma una foto después de cada evento y se
 *     promedian. Así una ejecución no se juzga solo por cómo terminó.
 */
typedef struct {
    /* Contadores de eventos */
    int    solicitudes;              /* eventos de asignación procesados      */
    int    asignadas;                /* asignaciones exitosas                 */
    int    liberaciones;             /* liberaciones exitosas                 */
    int    rechazos_sin_espacio;     /* no había memoria libre suficiente     */
    int    rechazos_fragmentacion;   /* había memoria, pero partida en huecos */
    int    errores;                  /* pid duplicado, inexistente, etc.      */

    /* Coste de búsqueda (bloques revisados por la estrategia) */
    long   revisados_exito;          /* suma en búsquedas que encontraron hueco */
    long   revisados_rechazo;        /* suma en búsquedas que no encontraron    */

    /* Sumas para promedios en el tiempo (una muestra por evento) */
    int    muestras;
    double suma_utilizacion;
    double suma_frag_externa;
    double suma_relacion_huecos;     /* solo muestras con algún ocupado        */
    int    muestras_relacion;
    double suma_frag_interna;        /* KB                                     */

    /* Picos */
    double max_frag_externa;
    int    max_huecos;
} Estadisticas;

/* Deja todos los contadores en cero */
void estadisticas_iniciar(Estadisticas *e);

/* Registra el resultado de una asignación (con su coste de búsqueda) */
void estadisticas_registrar_asignacion(Estadisticas *e, Resultado r, int revisados);

/* Registra el resultado de una liberación */
void estadisticas_registrar_liberacion(Estadisticas *e, Resultado r);

/* Suma una foto de la memoria a los promedios y picos */
void estadisticas_muestrear(Estadisticas *e, const Metricas *f);

/* Valores derivados (devuelven 0 si no hay datos, nunca dividen por 0) */
double estadisticas_tasa_rechazo(const Estadisticas *e);        /* rechazos / solicitudes */
double estadisticas_rev_prom_exito(const Estadisticas *e);      /* bloques por búsqueda exitosa */
double estadisticas_rev_prom_rechazo(const Estadisticas *e);    /* bloques por búsqueda fallida */
double estadisticas_utilizacion_prom(const Estadisticas *e);
double estadisticas_frag_externa_prom(const Estadisticas *e);
double estadisticas_relacion_huecos_prom(const Estadisticas *e);
double estadisticas_frag_interna_prom(const Estadisticas *e);

/* ---------- Secuencias de solicitudes ---------- */

typedef enum { EV_ASIGNAR, EV_LIBERAR } TipoEvento;

typedef struct {
    TipoEvento tipo;
    int        pid;
    int        tamano;   /* solo para EV_ASIGNAR; 0 en EV_LIBERAR */
} Evento;

typedef struct {
    Evento eventos[MAX_EVENTOS];
    int    n;
} Secuencia;   /* ~120 KB: declararla 'static', no como variable local */

/*
 * Lee un archivo con líneas:
 *   A <pid> <KB>   -> asignar
 *   L <pid>        -> liberar
 *   # comentario   -> se ignora (también las líneas vacías)
 * Los pid deben estar en [0, MAX_EVENTOS).
 * Devuelve false si el archivo tiene errores (informa la línea por stderr).
 */
bool secuencia_cargar(Secuencia *s, const char *ruta);

/*
 * Genera una secuencia aleatoria REPRODUCIBLE: la misma semilla produce
 * exactamente los mismos eventos, en cualquier computador (usa un generador
 * propio, no rand(), porque rand() cambia entre Linux y macOS).
 * Así las tres estrategias reciben la misma entrada.
 *
 * Modelo de carga: en cada paso, si hay 'v' procesos vivos (según el propio
 * generador), se libera uno al azar con probabilidad v / (v + vivos_objetivo);
 * si no, llega un proceso nuevo de tamaño uniforme en [tam_min, tam_max].
 * Con v == vivos_objetivo la probabilidad es 1/2, así que el número de
 * procesos vivos oscila alrededor de 'vivos_objetivo'.
 *
 *   Carga ofrecida ~= vivos_objetivo * tamaño medio / memoria total
 *
 * El generador no ve la memoria: si un proceso fue rechazado por el simulador,
 * su liberación posterior se ignora (ver main.c).
 */
void secuencia_generar(Secuencia *s, unsigned int semilla, int num_eventos,
                       int tam_min, int tam_max, int vivos_objetivo);

/* Número pseudoaleatorio portable (xorshift32). Estado != 0. */
unsigned int aleatorio_siguiente(unsigned int *estado);

#endif /* MEMORIA_H */
