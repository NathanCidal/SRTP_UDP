# Makefile de Exemplo de Aplicacao UDP

CC=gcc
SRC=main.c
TRG=executavel.out

all: build

build:
	$(CC) $(SRC) -o $(TRG)

client:
	./$(TRG) --host
server:
	./$(TRG) --listen
clean:
	rm -fr *.out