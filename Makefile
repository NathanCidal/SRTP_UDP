CC=gcc
FLAGS= -lz -DDEBUG	
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

# On Linux, Commands:

# Packet Loss:
# sudo tc qdisc add dev lo root netem loss 25%

# Pure Delay:
# sudo tc qdisc add dev lo root netem delay 50ms

# Change order:
# sudo tc qdisc add dev lo root netem delay 10ms reorder 25% 50%

# To Reset to default (network):
# sudo tc qdisc del dev dev lo root