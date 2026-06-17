# Makefile de Exemplo de Aplicacao UDP

CC=gcc
SRC=main.c
TRG=executavel.out
FLAGS= -lz # -DPRINT	

all: build

build:
	$(CC) $(SRC) $(FLAGS) -o $(TRG)

client: build
	./$(TRG) --host

server: build
	./$(TRG) --listen
clean:
	rm -fr *.out