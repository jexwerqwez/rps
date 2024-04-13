/**
 * @file client.cpp
 * @brief Клиентская часть для работы с файловым сервером.
 */
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>
#define BUFFER_SIZE 4096

/**
 * @struct ClientConfig
 * @brief Структура для хранения конфигурации клиента.
 *
 * @var ClientConfig::id Идентификатор клиента.
 * @var ClientConfig::server_address IP-адрес сервера.
 * @var ClientConfig::port Порт сервера.
 * @var ClientConfig::files Список файлов для передачи.
 */

struct ClientConfig {
  int id = 0;
  std::string server_address = "";
  int port = 0;
  std::vector<std::string> files;
};

ClientConfig readClientConfig(const std::string& filename);
void getAndProcessFileSize(int sock, ClientConfig& config);
void receiveFileData(int sock, const std::string& filePath);

/**
 * @brief Главная функция клиента для передачи файлов.
 *
 * @param argc Количество аргументов командной строки.
 * @param argv Массив аргументов командной строки.
 * @return Код завершения программы.
 */

int main(int argc, char** argv) {
  int sock = 0, valread;
  struct sockaddr_in serv_addr;
  char buffer[BUFFER_SIZE] = {0};

  if (argc < 2) {
    std::cerr << "Usage: ./client <config_file>" << std::endl;
    return -1;
  }

  ClientConfig config = readClientConfig(argv[1]);

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
  for (const auto& file : config.files) {
    send(sock, file.c_str(), file.length(), 0);
    std::cout << "File " << file << " sent" << std::endl;
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

      std::ifstream localFile(file, std::ifstream::ate | std::ifstream::binary);
      if (!localFile && exists) {
        // Если файл не открыт, но существует на сервере, создаём его
        std::ofstream newFile(file, std::ios::binary);
        newFile.close();
        localFile.open(file, std::ifstream::ate | std::ifstream::binary);
      }
      std::cout << "localFile " << file << std::endl;
      if (localFile) {
        double localFileSize = localFile.tellg();
        std::cout << "Local file size: " << localFileSize << " bytes"
                  << std::endl;

        if (localFileSize == serverFileSize) {
          std::string readyMessage = "SENDING DATA";
          std::cout << readyMessage << std::endl;
          send(sock, readyMessage.c_str(), readyMessage.length(), 0);
          receiveFileData(sock, file);
        } else {
          std::ostringstream resumeMsg;
          resumeMsg << "RESUME DOWNLOAD " << file << " " << localFileSize;
          std::cout << localFileSize << std::endl;
          std::cout << "Requesting file resume from byte: " << localFileSize
                    << std::endl;
          send(sock, resumeMsg.str().c_str(), resumeMsg.str().length(), 0);
          receiveFileData(sock, file);
        }
      }
    }
  }
  close(sock);
  return 0;
}

/**
 * @brief Читает конфигурацию клиента из файла.
 *
 * @param filename Имя файла конфигурации.
 * @return Структура конфигурации клиента.
 */

ClientConfig readClientConfig(const std::string& filename) {
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

/**
 * @brief Инициирует запрос на получение размера файла.
 *
 * @param sock Дескриптор сокета.
 * @param config Конфигурация клиента.
 */

void getAndProcessFileSize(int sock, ClientConfig& config) {
  send(sock, std::to_string(config.id).c_str(),
       sizeof(std::to_string(config.id)).length(), 0);

  sleep(1);
}

/**
 * @brief Получает данные файла от сервера и записывает их в файл.
 *
 * @param sock Дескриптор сокета.
 * @param filePath Путь к файлу для сохранения данных.
 */

void receiveFileData(int sock, const std::string& filePath) {
  std::cout << "Starting file download: " << filePath << std::endl;
  std::ofstream file(filePath, std::ios::binary | std::ios::app);
  if (!file.is_open()) {
    std::cerr << "Failed to open file: " << filePath << std::endl;
    return;
  }

  char buffer[BUFFER_SIZE];
  int bytesReceived;
  bool endOfDataReceived = false;
  size_t totalReceived = 0;
  const int numBlocks = 50;

  // Чтение размера файла
  bytesReceived = recv(sock, buffer, BUFFER_SIZE, 0);
  std::string sizeStr(buffer, bytesReceived);
  size_t fileSize = std::stoul(sizeStr.substr(0, sizeStr.find('\n')));
  std::cout << "Expected file size: " << fileSize << " bytes" << std::endl;

  while ((bytesReceived = recv(sock, buffer, sizeof(buffer), 0)) > 0) {
    usleep(10);

    file.write(buffer, bytesReceived);
    totalReceived += bytesReceived;
    if (strstr(buffer, "END_OF_DATA") ||
        totalReceived >= fileSize) {  // Проверка на наличие маркера завершения
      endOfDataReceived = true;
      std::cout << std::endl;
      break;
    }

    // Обновление прогресс-бара
    int progress = ceil(((double)totalReceived / (double)fileSize) * numBlocks);
    std::cout << "\r[";
    for (int i = 0; i < numBlocks; ++i) {
      std::cout << (i < progress ? "=" : " ");
    }
    std::cout << "] " << (int)(((double)totalReceived / (double)fileSize) * 100)
              << "%" << std::flush;
  }

  // Обеспечиваем, что прогресс-бар достигает 100%
  if (endOfDataReceived) {
    std::cout << "\r[";
    for (int i = 0; i < numBlocks; ++i) {
      std::cout << "=";
    }
    std::cout << "] 100%" << std::endl;
    std::cout << "All data received for this file." << std::endl;
  }

  file.close();
}
