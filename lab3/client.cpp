#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>

#define PORT 3456
#define SERVER_ADDRESS "127.0.0.1"

struct ClientConfig {
  std::string server_address = "";
  int port = 0;
  std::vector<std::string> files = {""};
};

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
      if (key == "server_address") {
        config.server_address = value.substr(1);
      } else if (key == "port") {
        config.port = std::stoi(value.substr(1));
      } else if (key == "files") {
        std::istringstream filestream(value);
        std::string file;
        while (filestream >> file) config.files.push_back(file);
      }
    }
  }
  file.close();
  return config;
}

int main(int argc, char** argv) {
  int sock = 0, valread;
  struct sockaddr_in serv_addr;
  char* hello = "Hello from client";
  char buffer[1024] = {0};
  if (argc < 2) {
    perror("Usage: ./client <config_file>");
    exit(EXIT_FAILURE);
  }

  ClientConfig config = readConfig(argv[1]);

  if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    std::cerr << "Socket creation error" << std::endl;
    return -1;
  }

  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(PORT);

  if (inet_pton(AF_INET, SERVER_ADDRESS, &serv_addr.sin_addr) <= 0) {
    std::cerr << "Invalid address/ Address not supported" << std::endl;
    return -1;
  }

  if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
    std::cerr << "Connection Failed" << std::endl;
    return -1;
  }

  send(sock, hello, strlen(hello), 0);
  std::cout << "Hello message sent" << std::endl;
  valread = read(sock, buffer, 1024);
  std::cout << buffer << std::endl;
  std::cout << "Server Address: " << config.server_address << std::endl;
  std::cout << "Port: " << config.port << std::endl;
  std::cout << "Files: ";

  for (const auto& file : config.files) std::cout << file << " ";

  close(sock);

  return 0;
}
