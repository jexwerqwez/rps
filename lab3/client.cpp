#include <iostream>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>

int main() {
    int sock = 0;
    struct sockaddr_in server_address;
    char buffer[1024] = {0};


    // создание файлового дескриптора сокета
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }
    // привязка сокета к порту
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(8080);

    if (connect(sock, (struct sockaddr *)&server_address, sizeof(server_address)) < 0) {
        perror("connect failed");
        exit(EXIT_FAILURE);
    }
    char* message = "client message";
    send(sock, message, strlen(message), 0);
    int valread = read(sock, buffer, 1024);

    printf("%s\n", buffer);
    printf("client message sent\n");

    return 0;
}