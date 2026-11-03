# Makefile — Simulador de gestión de memoria (Proyecto final SO)
#
#   make           compila el simulador (./simulador)
#   make test      compila y ejecuta todas las pruebas
#   make sanitize  igual, pero con AddressSanitizer y UBSan
#                  (detectan accesos fuera del arreglo y comportamiento indefinido)
#   make clean     borra lo compilado

CC      = gcc
CFLAGS  = -std=c11 -Wall -Wextra -pedantic -Werror -O2 -Iinclude
SANFLAGS = -fsanitize=address,undefined -fno-omit-frame-pointer -g

NUCLEO  = src/memoria.c src/invariantes.c src/estrategias.c src/metricas.c src/secuencia.c
BUILD   = build

.PHONY: all test sanitize clean

all: simulador

simulador: src/main.c $(NUCLEO) include/memoria.h
	$(CC) $(CFLAGS) src/main.c $(NUCLEO) -o $@

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/test_contrato: tests/test_contrato.c include/memoria.h | $(BUILD)
	$(CC) $(CFLAGS) $< -o $@

$(BUILD)/test_nucleo: tests/test_nucleo.c $(NUCLEO) include/memoria.h | $(BUILD)
	$(CC) $(CFLAGS) tests/test_nucleo.c $(NUCLEO) -o $@

$(BUILD)/test_estrategias: tests/test_estrategias.c $(NUCLEO) include/memoria.h | $(BUILD)
	$(CC) $(CFLAGS) tests/test_estrategias.c $(NUCLEO) -o $@

$(BUILD)/test_metricas: tests/test_metricas.c $(NUCLEO) include/memoria.h | $(BUILD)
	$(CC) $(CFLAGS) tests/test_metricas.c $(NUCLEO) -o $@ -lm

$(BUILD)/test_secuencia: tests/test_secuencia.c $(NUCLEO) include/memoria.h | $(BUILD)
	$(CC) $(CFLAGS) tests/test_secuencia.c $(NUCLEO) -o $@

test: $(BUILD)/test_contrato $(BUILD)/test_nucleo $(BUILD)/test_estrategias $(BUILD)/test_metricas $(BUILD)/test_secuencia
	./$(BUILD)/test_contrato
	./$(BUILD)/test_nucleo
	./$(BUILD)/test_estrategias
	./$(BUILD)/test_metricas
	./$(BUILD)/test_secuencia

sanitize: | $(BUILD)
	$(CC) $(CFLAGS) $(SANFLAGS) tests/test_nucleo.c $(NUCLEO) -o $(BUILD)/test_nucleo_san
	./$(BUILD)/test_nucleo_san
	$(CC) $(CFLAGS) $(SANFLAGS) tests/test_estrategias.c $(NUCLEO) -o $(BUILD)/test_estrategias_san
	./$(BUILD)/test_estrategias_san
	$(CC) $(CFLAGS) $(SANFLAGS) tests/test_metricas.c $(NUCLEO) -o $(BUILD)/test_metricas_san -lm
	./$(BUILD)/test_metricas_san
	$(CC) $(CFLAGS) $(SANFLAGS) tests/test_secuencia.c $(NUCLEO) -o $(BUILD)/test_secuencia_san
	./$(BUILD)/test_secuencia_san

clean:
	rm -rf $(BUILD) simulador
