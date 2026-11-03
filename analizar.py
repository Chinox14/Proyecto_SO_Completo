"""
analizar.py — Resume resultados/resultados.csv y genera las tablas y gráficos del informe.

    python3 analizar.py

Necesita pandas y matplotlib. Produce en resultados/:
  - tabla_resumen.csv        media y desviación estándar de cada métrica (10 semillas)
  - comparacion_pareada.csv  en cuántas semillas cada estrategia gana a primer ajuste
  - g1_rechazo.png ... g6_regla50.png
"""
import pandas as pd
import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt

R = "resultados/"
df = pd.read_csv(R + "resultados.csv")
serie = pd.read_csv(R + "serie_tiempo.csv")

ESTR = ["first", "best", "worst"]
NOMBRE = {"first": "Primer ajuste", "best": "Mejor ajuste", "worst": "Peor ajuste"}
COLOR = {"first": "#2a78d6", "best": "#eb6834", "worst": "#1baf7a"}
ESC = ["E1_baja", "E2_media", "E3_alta"]
ESC_TXT = {"E1_baja": "Carga baja\n(~34 %)", "E2_media": "Carga media\n(~69 %)",
           "E3_alta": "Carga alta\n(~112 %)"}

METRICAS = ["tasa_rechazo", "utilizacion_prom", "frag_externa_prom", "frag_interna_prom_kb",
            "relacion_huecos_prom", "revisados_exito_prom", "revisados_rechazo_prom"]

# ---------- Tabla resumen: media y desviación sobre las 10 semillas ----------
g = df.groupby(["escenario", "unidad", "estrategia"])[METRICAS]
resumen = g.mean().add_suffix("_media").join(g.std().add_suffix("_desv")).reset_index()
resumen.to_csv(R + "tabla_resumen.csv", index=False, float_format="%.4f")

# ---------- Comparación pareada (misma semilla = misma secuencia) ----------
filas = []
for (esc, uni), grupo in df.groupby(["escenario", "unidad"]):
    piv = grupo.pivot(index="semilla", columns="estrategia")
    for otra in ["best", "worst"]:
        for met, menor_es_mejor in [("tasa_rechazo", True), ("frag_externa_prom", True),
                                    ("utilizacion_prom", False)]:
            dif = piv[met][otra] - piv[met]["first"]
            gana = (dif < 0).sum() if menor_es_mejor else (dif > 0).sum()
            filas.append({"escenario": esc, "unidad": uni, "estrategia": otra, "metrica": met,
                          "diferencia_media_vs_first": dif.mean(),
                          "semillas_donde_gana_a_first": int(gana), "semillas": len(dif)})
pd.DataFrame(filas).to_csv(R + "comparacion_pareada.csv", index=False, float_format="%.5f")

# ---------- Estilo común ----------
plt.rcParams.update({
    "font.size": 10, "axes.spines.top": False, "axes.spines.right": False,
    "axes.edgecolor": "#8a8984", "axes.labelcolor": "#52514e", "xtick.color": "#52514e",
    "ytick.color": "#52514e", "axes.grid": True, "grid.color": "#e6e5e0", "grid.linewidth": 0.6,
    "axes.axisbelow": True, "figure.dpi": 150, "savefig.bbox": "tight",
})


def barras(metrica, unidad, titulo, ylabel, archivo, porcentaje=False, referencia=None):
    """Barras agrupadas por escenario, una por estrategia, con barra de error = 1 desviación."""
    sub = resumen[resumen.unidad == unidad]
    fig, ax = plt.subplots(figsize=(7, 3.8))
    ancho = 0.26
    for j, e in enumerate(ESTR):
        fila = sub[sub.estrategia == e].set_index("escenario").loc[ESC]
        y = fila[metrica + "_media"] * (100 if porcentaje else 1)
        err = fila[metrica + "_desv"] * (100 if porcentaje else 1)
        x = [i + (j - 1) * (ancho + 0.02) for i in range(len(ESC))]
        ax.bar(x, y, width=ancho, color=COLOR[e], label=NOMBRE[e], yerr=err,
               error_kw={"elinewidth": 1, "ecolor": "#52514e", "capsize": 2})
        for xi, yi, ei in zip(x, y, err):
            texto = f"{yi:.1f}" if (porcentaje or yi >= 10) else f"{yi:.2f}"
            ax.annotate(texto, (xi, yi + ei), ha="center", va="bottom", fontsize=7.5,
                        xytext=(0, 2), textcoords="offset points", color="#0b0b0b")
    if referencia is not None:
        ax.axhline(referencia, color="#52514e", linestyle="--", linewidth=1)
        ax.annotate(f"regla del 50 % = {referencia}", (-0.45, referencia), ha="left",
                    va="top", fontsize=8, color="#52514e", xytext=(0, -3),
                    textcoords="offset points",
                    bbox={"facecolor": "white", "edgecolor": "none", "pad": 1})
    ax.set_xticks(range(len(ESC)))
    ax.set_xticklabels([ESC_TXT[e] for e in ESC])
    ax.set_ylabel(ylabel)
    ax.set_title(titulo, loc="left", fontsize=11, color="#0b0b0b")
    ax.grid(axis="x", visible=False)
    ax.legend(frameon=False, ncol=3, loc="upper left", fontsize=9)
    ax.set_ylim(0, ax.get_ylim()[1] * 1.15)
    fig.savefig(R + archivo)
    plt.close(fig)


barras("tasa_rechazo", 1, "Solicitudes rechazadas (unidad 1 KB, media de 10 semillas)",
       "% de solicitudes rechazadas", "g1_rechazo.png", porcentaje=True)
barras("frag_externa_prom", 1, "Fragmentación externa promedio (unidad 1 KB)",
       "1 − hueco mayor / libre", "g2_frag_externa.png")
barras("revisados_exito_prom", 1, "Coste de búsqueda en asignaciones exitosas (unidad 1 KB)",
       "bloques revisados por búsqueda", "g3_coste_busqueda.png")
barras("relacion_huecos_prom", 1, "Huecos por bloque ocupado: regla del 50 % (unidad 1 KB)",
       "huecos / bloques ocupados", "g6_regla50.png", referencia=0.5)

# ---------- Efecto de la unidad de asignación (carga media) ----------
sub = resumen[resumen.escenario == "E2_media"]
fig, axes = plt.subplots(1, 2, figsize=(9, 3.6))
for ax, (met, tit, yl, pct) in zip(axes, [
        ("frag_interna_prom_kb", "Fragmentación interna", "KB desperdiciados dentro de particiones", False),
        ("tasa_rechazo", "Solicitudes rechazadas", "% de solicitudes", True)]):
    for e in ESTR:
        f = sub[sub.estrategia == e].sort_values("unidad")
        y = f[met + "_media"] * (100 if pct else 1)
        ax.plot(f.unidad, y, marker="o", markersize=6, linewidth=2, color=COLOR[e], label=NOMBRE[e])
    ax.set_xscale("log", base=2)
    ax.set_xticks([1, 4, 16])
    ax.set_xticklabels(["1 KB", "4 KB", "16 KB"])
    ax.set_xlabel("unidad de asignación")
    ax.set_ylabel(yl)
    ax.set_title(tit + " según la unidad (carga media)", loc="left", fontsize=10, color="#0b0b0b")
    ax.set_ylim(bottom=0)
axes[0].legend(frameon=False, fontsize=8.5)
fig.tight_layout()
fig.savefig(R + "g4_efecto_unidad.png")
plt.close(fig)

# ---------- Evolución en el tiempo (carga alta, semilla 1) ----------
fig, ax = plt.subplots(figsize=(9, 3.6))
for e in ESTR:
    s = serie[serie.estrategia == e].sort_values("evento")
    ax.plot(s.evento, s.frag_externa.rolling(200, min_periods=1).mean(), linewidth=2,
            color=COLOR[e], label=NOMBRE[e])
ax.set_xlabel("evento (solicitud o liberación)")
ax.set_ylabel("fragmentación externa\n(media móvil de 200 eventos)")
ax.set_title("Evolución de la fragmentación externa (carga alta, unidad 1 KB, semilla 1)",
             loc="left", fontsize=10.5, color="#0b0b0b")
ax.set_ylim(0, 1)
ax.legend(frameon=False, ncol=3, loc="upper left", fontsize=9)
fig.savefig(R + "g5_evolucion.png")
plt.close(fig)

# ---------- Resumen en consola ----------
pd.set_option("display.width", 200)
cols = ["escenario", "unidad", "estrategia", "tasa_rechazo_media", "tasa_rechazo_desv",
        "utilizacion_prom_media", "frag_externa_prom_media", "frag_interna_prom_kb_media",
        "relacion_huecos_prom_media", "revisados_exito_prom_media", "revisados_rechazo_prom_media"]
print(resumen[cols].round(3).to_string(index=False))
print()
print(pd.read_csv(R + "comparacion_pareada.csv").round(4).to_string(index=False))
