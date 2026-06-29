CC=gcc
FLAGS= -lz # -DDEBUG	
SRC=main.c srtp.c parser.c
INCLUDE=srtp.h
TRG=executavel.out

PORT=6000
FILE_NAME=input/input_01.txt
IP=127.0.0.1
SIZE=4
MODE=saw

all: build

build:
	@$(CC) $(SRC) $(FLAGS) -o $(TRG)

client: build
	./$(TRG) --host $(IP) --port $(PORT) --file $(FILE_NAME) --size $(SIZE) --mode $(MODE)

server: build
	./$(TRG) --listen --port $(PORT) --size $(SIZE) --mode $(MODE)
clean:
	rm -fr *.out
	rm -fr output_file.txt