# Makefile de Exemplo de Aplicacao UDP

CC=gcc
FLAGS= -lz -DDEBUG	
SRC=main.c srtp.c parser.c
INCLUDE=srtp.h
TRG=executavel.out

PORT=6000
FILE_NAME=test_input_01.txt
IP=127.0.0.1

all: build

build:
	@$(CC) $(SRC) $(FLAGS) -o $(TRG)

client: build
	./$(TRG) --host $(IP) --port $(PORT) --file $(FILE_NAME)

server: build
	./$(TRG) --listen --port $(PORT)
clean:
	rm -fr *.out