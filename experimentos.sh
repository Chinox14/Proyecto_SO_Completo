#!/bin/sh
# experimentos.sh — Ejecuta TODOS los experimentos del informe y guarda los resultados.
#
#   sh experimentos.sh
#
# Escenarios (misma memoria de 256 KB, procesos de 4 a 40 KB, 5000 eventos):
#   E1 carga baja   -> 4 procesos vivos en promedio   (carga ofrecida ~34 %)
#   E2 carga media  -> 8 procesos vivos en promedio   (carga ofrecida ~69 %)
#   E3 carga alta   -> 13 procesos vivos en promedio  (carga ofrecida ~112 %)
# Cada escenario se corre con unidades de 1, 4 y 16 KB y con 10 semillas (1..10).
# Para cada combinación, las 3 estrategias reciben EXACTAMENTE la misma secuencia.

set -e

make -s simulador
mkdir -p resultados
rm -f resultados/resultados.csv resultados/serie_tiempo.csv

for escenario in "E1_baja 4" "E2_media 8" "E3_alta 13"; do
    set -- $escenario
    nombre=$1
    vivos=$2
    for unidad in 1 4 16; do
        for semilla in 1 2 3 4 5 6 7 8 9 10; do
            ./simulador --escenario "$nombre" --vivos "$vivos" --unidad "$unidad" \
                        --semilla "$semilla" --eventos 5000 --tmin 4 --tmax 40 \
                        --csv resultados/resultados.csv > /dev/null
        done
    done
    echo "Escenario $nombre listo"
done

# Serie en el tiempo para los gráficos de evolución
./simulador --escenario E3_alta --vivos 13 --unidad 1 --semilla 1 --eventos 5000 \
            --serie resultados/serie_tiempo.csv > /dev/null

# Evidencias: casos conocidos con traza completa
./simulador --archivo datos/ejemplo_apuntes.txt --memoria 100 --escenario apuntes --traza \
            > resultados/traza_ejemplo_apuntes.txt
./simulador --archivo datos/ejemplo_quiz.txt --memoria 100 --escenario quiz --traza \
            > resultados/traza_ejemplo_quiz.txt

# Evidencia: una tabla resumen por escenario (semilla 1, unidad 1)
{
    ./simulador --escenario E1_baja  --vivos 4  --semilla 1
    ./simulador --escenario E2_media --vivos 8  --semilla 1
    ./simulador --escenario E3_alta  --vivos 13 --semilla 1
} > resultados/resumen_semilla1.txt

echo "Listo: $(($(wc -l < resultados/resultados.csv) - 1)) filas en resultados/resultados.csv"
