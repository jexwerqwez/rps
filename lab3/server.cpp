#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <vector>

#define BUFFER_SIZE 1024

struct ServerConfig {
  std::string server_address = "";
  int port = 0;
  std::string directory = "";
};

ServerConfig readConfig(const std::string& filename);
void checkDirectory(ServerConfig& config);
void checkFileExistance(ServerConfig& config, int client_id, int new_socket);
void checkFileStatus(std::fstream& file, int new_socket);
float readClientFile(std::fstream& file, std::string buffer);

int main(int argc, char** argv) {
  int server_fd, new_socket, valread;
  struct sockaddr_in address;
  int opt = 1;
  int addrlen = sizeof(address);
  // char buffer[BUFFER_SIZE] = {0};

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

  if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
    std::cerr << argv[0] << ": bind failed" << std::endl;
    return -1;
  }

  if (listen(server_fd, 3) < 0) {
    std::cerr << argv[0] << ": listen failed" << std::endl;
    return -1;
  }

  while (1) {
    if ((new_socket = accept(server_fd, (struct sockaddr*)&address,
                             (socklen_t*)&addrlen)) < 0) {
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
      checkFileExistance(config, client_id, new_socket);

      char readyBuffer[BUFFER_SIZE] = {0};
      int messageLength = read(new_socket, readyBuffer, BUFFER_SIZE);
      if (messageLength > 0) {
        std::string clientMessage(readyBuffer, messageLength);
        std::cout << "Client message: " << clientMessage << std::endl;
      }
      close(new_socket);
      exit(0);
    } else {
      close(new_socket);
    }
  }

  return 0;
}

void checkDirectory(ServerConfig& config) {
  struct stat st = {0};
  if (stat(config.directory.c_str(), &st) == -1) {
    std::cout << "Directory does not exist. Creating: " << config.directory
              << std::endl;
    mkdir(config.directory.c_str(), 0700);
  }
}

void checkFileExistance(ServerConfig& config, int client_id, int new_socket) {
  std::string file_path =
      config.directory + "/client_" + std::to_string(client_id) + ".txt";
  std::fstream file(
      file_path, std::fstream::in | std::fstream::out |
                     std::fstream::app);  // Открытие файла для чтения и записи
  if (file.good()) {
    std::cout << "File " << file_path << " exists." << std::endl;
  } else {
    std::cout << "File " << file_path << " does not exist. Creating file..."
              << std::endl;
    file.open(file_path, std::fstream::out);
    file << "Client ID: " << client_id << std::endl;
    file.close();
    file.open(file_path, std::fstream::in | std::fstream::out |
                             std::fstream::app);  // Повторное открытие файла
    if (file.good()) {
      std::cout << "File created successfully." << std::endl;
    } else {
      std::cerr << "Failed to create file." << std::endl;
    }
  }
  checkFileStatus(file, new_socket);
}

float readClientFile(std::fstream& file, std::string buffer) {
  std::string line;
  while (std::getline(file, line)) {
    std::istringstream iss(line);
    std::string key;
    float value;
    if (std::getline(iss, key, ':') && key == buffer && iss >> value)
      return value;
  }
  return -1;
}

void checkFileStatus(std::fstream& file, int new_socket) {
  char buffer[BUFFER_SIZE] = {0};
  memset(buffer, 0, BUFFER_SIZE);
  int valread = read(new_socket, buffer, BUFFER_SIZE);
  if (valread > 0) {
    buffer[valread] = '\0';
    std::string clientFileName(buffer);
    std::cout << "Received file name: " << clientFileName << std::endl;

    // Получаем размер файла из файла данных
    float fileSize = readClientFile(file, clientFileName);
    if (fileSize >= 0) {
      // Если файл найден, отправляем true и размер файла
      std::string response = "true " + std::to_string(fileSize);
      send(new_socket, response.c_str(), response.length(), 0);
    } else {
      // Если файла нет, отправляем false 0
      std::string response = "false 0";
      send(new_socket, response.c_str(), response.length(), 0);
    }
  }
}

ServerConfig readConfig(const std::string& filename) {
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
