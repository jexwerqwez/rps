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
#include <map>
#include <sstream>
#include <string>
#include <vector>

#define BUFFER_SIZE 1024

struct ServerConfig {
  std::string server_address = "";
  int port = 0;
  std::string directory = "";
};

ServerConfig readConfig(const std::string& filename);
void checkDirectory(ServerConfig& config);
std::fstream checkFileExistance(ServerConfig& config, int client_id,
                                int new_socket);
std::string checkFileStatus(std::fstream& progress_file, int new_socket,
                            int client_id, ServerConfig& config);
float readClientFile(std::fstream& file, std::string buffer);
void sendFileData(ServerConfig& config, int client_id, int new_socket,
                  const std::string& file_path);

int main(int argc, char** argv) {
  int server_fd, new_socket, valread;
  struct sockaddr_in address;
  int opt = 1;
  int addrlen = sizeof(address);

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
      std::fstream file = checkFileExistance(config, client_id, new_socket);
      std::string recieved_file =
          checkFileStatus(file, new_socket, client_id, config);
      if (!recieved_file.empty()) {
        char readyBuffer[BUFFER_SIZE] = {0};
        int messageLength = read(new_socket, readyBuffer, BUFFER_SIZE);
        std::string clientMessage(readyBuffer, messageLength);
        std::cout << "Client message: " << clientMessage << std::endl;
        if (clientMessage == "SENDING DATA") {
          std::string clientMessage(readyBuffer, messageLength);
          std::cout << "Client message: " << clientMessage << std::endl;
          sendFileData(config, client_id, new_socket,
                       "server_files/" + recieved_file);
        }
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

std::fstream checkFileExistance(ServerConfig& config, int client_id,
                                int new_socket) {
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
  return file;
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

std::string checkFileStatus(std::fstream& file, int new_socket, int client_id,
                            ServerConfig& config) {
  char buffer[BUFFER_SIZE] = {0};
  memset(buffer, 0, BUFFER_SIZE);
  int valread = read(new_socket, buffer, BUFFER_SIZE);
  if (valread > 0) {
    buffer[valread] = '\0';
    std::string clientFileName(buffer);
    std::cout << "Received file name: " << clientFileName << std::endl;

    // Проверка наличия файла в server_files/
    std::string serverFilePath = "server_files/" + clientFileName;
    bool fileExistsOnServer = std::ifstream(serverFilePath).good();

    // Проверка записи о файле в client_*.txt
    float fileSize = readClientFile(file, clientFileName);
    if (fileExistsOnServer && fileSize < 0) {
      // Добавление записи, если файла нет в client_*.txt
      file << clientFileName << " : " << 0 << std::endl;
      fileSize = 0;
    }

    // Отправка данных клиенту
    std::string response =
        fileExistsOnServer ? "true " + std::to_string(fileSize) : "false 0";
    send(new_socket, response.c_str(), response.length(), 0);

    return clientFileName;
  }
  return "";
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

void sendFileData(ServerConfig& config, int client_id, int new_socket,
                  const std::string& file_name) {
  std::string file_path = file_name;
  std::cout << "Sending file data: " << file_path << std::endl;

  std::ifstream file(file_path, std::ios::binary);
  if (!file.is_open()) {
    std::cerr << "Failed to open file: " << file_path << std::endl;
    return;
  }

  char buffer[4096];
  size_t sent_bytes = 0;
  while (!file.eof()) {
    file.read(buffer, sizeof(buffer));
    int bytes_to_send = file.gcount();
    send(new_socket, buffer, bytes_to_send, 0);
    sent_bytes += bytes_to_send;
    std::cout << "Total bytes sent for " << file_path << ": " << sent_bytes
              << std::endl;
  }

  file.close();

  // Update progress in the client progress file
  std::string progress_file_path =
      config.directory + "/client_" + std::to_string(client_id) + ".txt";
  std::fstream progress_file(progress_file_path, std::fstream::in |
                                                     std::fstream::out |
                                                     std::fstream::app);
  if (!progress_file.is_open()) {
    std::cerr << "Failed to open progress file: " << progress_file_path
              << std::endl;
    return;
  }

  std::string temp_line;
  std::map<std::string, size_t> file_sizes;
  while (std::getline(progress_file, temp_line)) {
    std::istringstream iss(temp_line);
    std::string temp_file_name;
    size_t temp_size;
    if (iss >> temp_file_name >> temp_size) {
      file_sizes[temp_file_name] = temp_size;
    }
  }
  file_sizes[file_name] = sent_bytes;
  progress_file.close();
  progress_file.open(progress_file_path,
                     std::fstream::out | std::fstream::trunc);
  for (const auto& pair : file_sizes) {
    progress_file << pair.first << " : " << pair.second << std::endl;
  }
  progress_file.close();
}
