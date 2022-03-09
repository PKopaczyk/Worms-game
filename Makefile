SRC=err.cpp crc.cpp read_line.cpp

CC = g++

CFLAGS=-std=c++17 -Wall -Wextra -O0
all: screen-worms-server screen-worms-client

screen-worms-server: screen-worms-server.cpp $(SRC)
	$(CC) $(CFLAGS) -o screen-worms-server screen-worms-server.cpp $(SRC) -lpthread

screen-worms-client: screen-worms-client.cpp $(SRC)
	$(CC) $(CFLAGS) -o screen-worms-client screen-worms-client.cpp $(SRC) -lpthread

clean: 
	rm -f screen-worms-client screen-worms-server