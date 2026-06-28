# Makefile de Exemplo de Aplicacao UDP

CC=gcc
FLAGS= -lz -DDEBUG	
SRC=main.c srtp.c
INCLUDE=srtp.h
TRG=executavel.out

all: build

build:
	@$(CC) $(SRC) $(FLAGS) -o $(TRG)

client: build
	./$(TRG) --host

server: build
	./$(TRG) --listen
clean:
	rm -fr *.out