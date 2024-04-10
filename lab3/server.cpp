#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>
#include <sys/stat.h>
#include <sys/types.h>

#define BUFFER_SIZE 1024

struct ServerConfig {
  std::string server_address = "";
  int port = 0;
  std::string directory = "";
};

void checkDirectory(ServerConfig& config) {
  struct stat st = {0};
  if (stat(config.directory.c_str(), &st) == -1) {
    std::cout << "Directory does not exist. Creating: " << config.directory << std::endl;
    mkdir(config.directory.c_str(), 0700);  // Установите необходимые права доступа
  }
}

void checkFile(ServerConfig& config, int client_id) {
  std::string file_path = config.directory + "/client_" + std::to_string(client_id) + ".txt";
  std::ifstream file(file_path);
  if (file.good()) {
    std::cout << "File " << file_path << " exists." << std::endl;
  } else {
    std::cout << "File " << file_path << " does not exist. Creating file..." << std::endl;
    std::ofstream outfile(file_path);
    outfile << "Client ID: " << client_id << std::endl;
    outfile.close();
    if (outfile.good()) {
      std::cout << "File created successfully." << std::endl;
    } else {
      std::cerr << "Failed to create file." << std::endl;
    }
  }
}

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
  int server_fd, new_socket, valread;
  struct sockaddr_in address;
  int opt = 1;
  int addrlen = sizeof(address);
  char buffer[BUFFER_SIZE] = {0};

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
  address.sin_addr.s_addr = inet_addr(config.server_address.c_str());
  address.sin_port = htons(config.port);

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
      char id_buffer[BUFFER_SIZE];
      memset(id_buffer, 0, BUFFER_SIZE);
      valread = read(new_socket, id_buffer, BUFFER_SIZE);
      std::string client_id_str(id_buffer, valread);
      int client_id = std::stoi(client_id_str);
      std::cout << "Client id: " << client_id << std::endl;
      checkDirectory(config);
      checkFile(config, client_id);
      // memset(buffer, 0, BUFFER_SIZE); // Очистка буфера
      // while ((valread = read(new_socket, buffer, BUFFER_SIZE)) > 0) {
      //   std::string file_path = std::string(buffer);
      //   bool exists = file.good();
      //   std::string response = exists ? "true" : "false";
      //   send(new_socket, response.c_str(), response.length(), 0);
      //   std::cout << "Received file name: " << buffer << std::endl;
      //   memset(buffer, 0, BUFFER_SIZE); // Повторная очистка буфера для следующего имени файла
      // }
      close(new_socket);
      exit(0);
    } else {
      close(new_socket);
    }
  }

  return 0;
}
