CC = g++
SERVER = $(wildcard server*.cpp)
CLIENT = $(wildcard client*.cpp)

SERVER_NAME = server
CLIENT_NAME = client

all: server.o client.o

server.o:
	$(CC) $(SERVER) -o $(SERVER_NAME)

client.o:
	$(CC) $(CLIENT) -o $(CLIENT_NAME)

start_server:
	./$(SERVER_NAME)

start_client:
	./$(CLIENT_NAME)