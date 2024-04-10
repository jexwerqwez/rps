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
      }else if (key == "server_address") {
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

int main(int argc, char** argv) {
  int sock = 0, valread;
  struct sockaddr_in serv_addr;
  char buffer[BUFFER_SIZE] = {0};
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
  serv_addr.sin_port = htons(config.port);

  if (inet_pton(AF_INET, config.server_address.c_str(), &serv_addr.sin_addr) <= 0) {
    std::cerr << "Invalid address/ Address not supported" << std::endl;
    return -1;
  }

  if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
    std::cerr << "Connection Failed" << std::endl;
    return -1;
  }
  send(sock, std::to_string(config.id).c_str(), sizeof(std::to_string(config.id)).length(), 0);
  sleep(1);
  // for (const auto& file : config.files) {
  //   // conts auto& file = config.files[0];
  //   send(sock, file.c_str(), file.length(), 0);
  //   memset(buffer, 0, BUFFER_SIZE);
  //   valread = read(sock, buffer, BUFFER_SIZE);
  //   std::cout << "File " << " exists on server: " << buffer << std::endl;
  //   sleep(1);
  // }

  close(sock);

  return 0;
}
 