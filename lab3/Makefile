CC = g++
SERVER = $(wildcard server*.cpp)
CLIENT = $(wildcard client*.cpp)

SERVER_NAME = server
SERVER_CONFIG = config_server
CLIENT_NAME = client
CLIENT_CONFIG = config_client

all: server.o client.o start_server

server.o:
	$(CC) $(SERVER) -o $(SERVER_NAME)

client.o:
	$(CC) $(CLIENT) -o $(CLIENT_NAME)

start_server:
	./$(SERVER_NAME) $(SERVER_CONFIG)

start_client:
	./$(CLIENT_NAME) $(CLIENT_CONFIG)