# Simulador de gestión de memoria — Particionado dinámico

Proyecto final de Sistemas Operativos, Opción 2 (gestión de memoria).

El programa simula la asignación contigua de memoria con **particionado dinámico**. Compara tres estrategias de asignación de huecos: **primer ajuste** (*first fit*), **mejor ajuste** (*best fit*) y **peor ajuste** (*worst fit*). Mide la fragmentación interna y externa, la utilización y el coste de búsqueda de cada una.

## Requisitos

- Linux o macOS con `gcc` y `make`. El código está en C11 y no usa bibliotecas externas.
- Para los gráficos se necesita Python 3 con `pandas` y `matplotlib`. Es opcional: el simulador funciona sin Python.

## Compilar y probar

```sh
make            # compila ./simulador
make test       # ejecuta todas las pruebas automáticas
make sanitize   # pruebas con AddressSanitizer y UBSan
```

## Uso del simulador

```sh
# Ejemplo de los apuntes (huecos de 15, 4, 20 y 13 KB; petición de 12 KB), paso a paso:
./simulador --archivo datos/ejemplo_apuntes.txt --memoria 100 --traza

# Secuencia aleatoria reproducible con las tres estrategias:
./simulador --semilla 7 --vivos 8 --unidad 4

# Una sola estrategia, guardando resultados en CSV:
./simulador --semilla 7 --estrategia best --csv mis_resultados.csv
```

| Opción | Significado | Por defecto |
|---|---|---|
| `--archivo RUTA` | Lee la secuencia de un archivo | (no se usa) |
| `--semilla N` | Semilla de la secuencia aleatoria | 1 |
| `--eventos N` | Número de eventos generados | 5000 |
| `--tmin N`, `--tmax N` | Tamaño mínimo y máximo de un proceso (KB) | 4 y 40 |
| `--vivos N` | Procesos vivos en promedio (controla la carga) | 9 |
| `--memoria KB` | Memoria total | 256 |
| `--unidad KB` | Unidad de asignación (bloque de tamaño fijo) | 1 |
| `--estrategia E` | `first`, `best`, `worst` o `todas` | todas |
| `--traza` | Muestra el mapa de memoria después de cada evento | no |
| `--csv RUTA` | Agrega una fila por estrategia al CSV | no |
| `--serie RUTA` | Guarda las métricas después de cada evento | no |
| `--escenario NOMBRE` | Etiqueta para el CSV | manual |

### Formato de los archivos de secuencia

```
# comentario
A 1 12      asignar 12 KB al proceso 1
L 1         liberar el proceso 1
```

## Reproducir los resultados del informe

```sh
sh experimentos.sh    # 270 ejecuciones: 3 cargas x 3 unidades x 10 semillas (x 3 estrategias)
python3 analizar.py   # tablas y gráficos en resultados/
```

Las tres estrategias reciben **siempre la misma secuencia** de solicitudes. El generador aleatorio es propio (xorshift32), así que **la misma semilla da los mismos resultados en cualquier computador**.

## Estructura

```
include/memoria.h     contrato: estructuras, funciones e invariantes
src/memoria.c         asignar (división del hueco) y liberar (fusión de huecos adyacentes)
src/estrategias.c     primer, mejor y peor ajuste
src/invariantes.c     verificación de consistencia después de cada operación (I1–I7)
src/metricas.c        fragmentación, utilización y coste de búsqueda
src/secuencia.c       lectura de archivos y generador reproducible
src/main.c            programa principal, tabla y CSV
tests/                pruebas automáticas
datos/                secuencias de prueba (ejemplo del quiz y de los apuntes)
resultados/           CSV, gráficos y trazas generados
```

## Verificación

- **Invariantes I1–I7** después de cada operación: bloques contiguos, suma igual al total, tamaños múltiplos de la unidad, sin huecos adyacentes sin fundir, pid únicos, fragmentación interna menor que una unidad y coherencia entre hueco y bloque ocupado. Si uno falla, el programa se detiene.
- **Pruebas automáticas** (`make test`): casos calculados a mano, ejemplo de los apuntes, un oráculo que compara cada estrategia con su definición en 90 000 búsquedas, y pruebas de estrés con un modelo paralelo.
- **Mutantes:** se introdujeron errores a propósito en el núcleo y en las métricas para comprobar que las pruebas los detectan.

## Limitaciones (simplificaciones declaradas)

- Si ningún hueco sirve, la solicitud se **rechaza** y se cuenta. Un SO real podría hacer esperar al proceso o atender primero a otro más pequeño de la cola de entrada.
- No hay compactación ni paginación.
- El tiempo no se simula: solo importa el orden de los eventos.
