#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>

#define PORT 3456
#define HOST INADDR_ANY

struct ServerConfig {
  std::string server_address = "";
  int port = 0;
  std::string directory = "";
};

ServerConfig readConfig(const std::string &filename) {
  ServerConfig config;
  std::ifstream file(filename);
  std::string line;
  if (!file.is_open()) {
    std::cerr << "Unable to open file: " << filename << std::endl;
    return config;
  }
  while (getline(file, line)) {
    std::istringstream iss(line);
    std::string key;
    if (getline(iss, key, ':')) {
      std::string value;
      getline(iss, value);
      if (key == "server_address")
        config.server_address = value.substr(1);
      else if (key == "port")
        config.port = std::stoi(value.substr(1));
      else if (key == "directory")
        config.directory = value.substr(1);
    }
  }
  file.close();
  return config;
}

int main(int argc, char **argv) {
  int server_fd, new_socket;
  struct sockaddr_in address;
  int opt = 1;
  int addrlen = sizeof(address);
  char buffer[1024] = {0};
  char *hello = "Hello from server";

  if (argc < 2) {
    std::cerr << "Usage: " << argv[0] << " <config_file>" << std::endl;
    return -1;
  }

  ServerConfig config = readConfig(argv[1]);

  std::cout << "Server Address: " << config.server_address << std::endl;
  std::cout << "Port: " << config.port << std::endl;
  std::cout << "Directory: " << config.directory << std::endl;

  if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
    perror("socket failed");
    exit(EXIT_FAILURE);
  }

  address.sin_family = AF_INET;
  address.sin_addr.s_addr = HOST;
  address.sin_port = htons(PORT);

  if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
    std::cerr << argv[0] << ": bind failed" << std::endl;
    return -1;
  }

  if (listen(server_fd, 3) < 0) {
    std::cerr << argv[0] << ": listen failed" << std::endl;
    return -1;
  }

  while (1) {
    if ((new_socket = accept(server_fd, (struct sockaddr *)&address,
                             (socklen_t *)&addrlen)) < 0) {
      std::cerr << argv[0] << ": accept failed" << std::endl;
      return -1;
    }

    int pid = fork();
    if (pid < 0) {
      std::cerr << argv[0] << ": fork failed" << std::endl;
      return -1;
    }

    // Дочерний процесс
    if (pid == 0) {
      close(server_fd);

      // Обработка данных от клиента
      memset(buffer, 0, 1024);  // Очистка буфера
      read(new_socket, buffer, 1024);
      printf("Message from client: %s\n", buffer);
      send(new_socket, hello, strlen(hello), 0);
      close(new_socket);

      exit(0);
    }
    // Родительский процесс
    else {
      close(new_socket);
    }
  }

  return 0;
}
