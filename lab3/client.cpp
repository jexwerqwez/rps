#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>
#define BUFFER_SIZE 1024

struct ClientConfig {
  int id = 0;
  std::string server_address = "";
  int port = 0;
  std::vector<std::string> files;
};

ClientConfig readConfig(const std::string& filename);
void getAndProcessFileSize(int sock, ClientConfig& config);
void receiveFileData(int sock, const std::string& filePath);

int main(int argc, char** argv) {
  int sock = 0, valread;
  struct sockaddr_in serv_addr;
  char buffer[BUFFER_SIZE] = {0};

  if (argc < 2) {
    std::cerr << "Usage: ./client <config_file>" << std::endl;
    return -1;
  }

  ClientConfig config = readConfig(argv[1]);

  if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    std::cerr << "Socket creation error" << std::endl;
    return -1;
  }

  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(config.port);

  if (inet_pton(AF_INET, config.server_address.c_str(), &serv_addr.sin_addr) <=
      0) {
    std::cerr << "Invalid address/ Address not supported" << std::endl;
    return -1;
  }

  if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
    std::cerr << "Connection Failed" << std::endl;
    return -1;
  }

  getAndProcessFileSize(sock, config);

  send(sock, config.files[0].c_str(), config.files[0].length(), 0);
  std::cout << "File " << config.files[0] << " sent" << std::endl;

  // Получаем размер файла от сервера
  memset(buffer, 0, BUFFER_SIZE);
  valread = read(sock, buffer, BUFFER_SIZE);

  std::string response(buffer, valread);
  std::istringstream iss(response);
  std::string existsStr;
  double serverFileSize;

  if (iss >> existsStr >> serverFileSize) {
    bool exists = existsStr == "true";
    std::cout << "File exists on server: " << std::boolalpha << exists
              << ", Server file size: " << serverFileSize << " bytes"
              << std::endl;

    std::ifstream localFile(config.files[0],
                            std::ifstream::ate | std::ifstream::binary);
    if (localFile) {
      double localFileSize = localFile.tellg();
      std::cout << "Local file size: " << localFileSize << " bytes"
                << std::endl;

      if (localFileSize == serverFileSize) {
        std::string readyMessage = "SENDING DATA";
        send(sock, readyMessage.c_str(), readyMessage.length(), 0);
        receiveFileData(sock, config.files[0]);
      } else {
        std::cerr << "Error: File size mismatch. Local: " << localFileSize
                  << ", Server: " << serverFileSize << std::endl;
      }
    } else {
      std::cerr << "Error: Unable to open the file." << std::endl;
    }
  } else {
    std::cerr << "Failed to parse server response: " << response << std::endl;
  }

  close(sock);
  return 0;
}

ClientConfig readConfig(const std::string& filename) {
  ClientConfig config;
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
      if (key == "id") {
        config.id = std::stoi(value.substr(1));
      } else if (key == "server_address") {
        config.server_address = value.substr(1);
      } else if (key == "port") {
        config.port = std::stoi(value.substr(1));
      } else if (key == "files") {
        std::istringstream filestream(value);
        std::string file;
        while (filestream >> file) {
          config.files.push_back(file);
        }
      }
    }
  }
  file.close();
  return config;
}

void getAndProcessFileSize(int sock, ClientConfig& config) {
  send(sock, std::to_string(config.id).c_str(),
       sizeof(std::to_string(config.id)).length(), 0);

  sleep(1);
}

void receiveFileData(int sock, const std::string& filePath) {
  std::cout << "receiveFileData " << filePath << std::endl;
  std::ofstream file(filePath, std::ios::binary | std::ios::app);
  char buffer[4096];
  int bytesReceived;

  if (!file.is_open()) {
    std::cerr << "Failed to open file: " << filePath << std::endl;
    return;
  }

  while ((bytesReceived = recv(sock, buffer, sizeof(buffer), 0)) > 0) {
    file.write(buffer, bytesReceived);
    std::cout << "Received " << bytesReceived << " bytes" << std::endl;
  }

  file.close();
}