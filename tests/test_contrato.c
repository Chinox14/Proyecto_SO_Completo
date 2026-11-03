/* Verifica que el contrato compila en C11 estricto y que es coherente. */
#include "memoria.h"
#include <stdio.h>

/* Los límites alcanzan para el peor caso de bloques */
_Static_assert(MAX_BLOQUES >= 2 * MAX_PROCESOS + 1,
               "Con P procesos puede haber hasta 2P+1 bloques (huecos entre ellos)");
_Static_assert(FIRST_FIT == 0 && WORST_FIT == 2, "Enumeración de estrategias");
_Static_assert(MEMORIA_DEFECTO % 16 == 0,
               "La memoria por defecto debe ser múltiplo de las unidades 1, 4 y 16");

/* Las tres estrategias tienen exactamente el tipo FnEstrategia
 * (_Generic no evalúa la expresión, así que no hace falta implementarlas aún) */
#define ES_ESTRATEGIA(f) _Generic(&(f), FnEstrategia: 1, default: 0)
_Static_assert(ES_ESTRATEGIA(elegir_first_fit), "first fit no cumple el contrato");
_Static_assert(ES_ESTRATEGIA(elegir_best_fit),  "best fit no cumple el contrato");
_Static_assert(ES_ESTRATEGIA(elegir_worst_fit), "worst fit no cumple el contrato");

int main(void) {
    printf("sizeof(Bloque)       = %zu bytes\n", sizeof(Bloque));
    printf("sizeof(Memoria)      = %zu bytes\n", sizeof(Memoria));
    printf("sizeof(Secuencia)    = %zu bytes\n", sizeof(Secuencia));
    printf("sizeof(Metricas)     = %zu bytes\n", sizeof(Metricas));
    printf("sizeof(Estadisticas) = %zu bytes\n", sizeof(Estadisticas));
    puts("Contrato OK");
    return 0;
}
