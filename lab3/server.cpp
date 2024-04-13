/**
 * @file server.cpp
 * @brief Описание серверной части приложения для обработки файловых запросов.
 */

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

/**
 * @struct ServerConfig
 * @brief Структура для хранения конфигурации сервера.
 *
 * @var ServerConfig::server_address IP-адрес сервера.
 * @var ServerConfig::port Порт сервера.
 * @var ServerConfig::directory Директория для файлов клиента.
 */
struct ServerConfig {
  std::string server_address = "";
  int port = 0;
  int buffer_size = 0;
  std::string directory = "";
};

ServerConfig readServerConfig(const std::string& filename);
void checkDirectory(ServerConfig& config);
std::fstream checkFileExistance(ServerConfig& config, int client_id,
                                int new_socket);
std::string checkFileStatus(std::fstream& progress_file, int new_socket,
                            int client_id, ServerConfig& config);
float readClientFile(std::fstream& file, const std::string& fileName);
void sendFileData(ServerConfig& config, int client_id, int new_socket,
                  const std::string& file_name, size_t startPos);
void updateProgressFile(const std::string& progressFilePath,
                        const std::string& fileName, size_t sentBytes);

/**
 * @brief Главная функция сервера.
 *
 * Инициализирует сервер, обрабатывает подключения клиентов и управляет
 * передачей данных.
 *
 * @param argc Количество аргументов командной строки.
 * @param argv Массив аргументов командной строки.
 * @return Код завершения программы.
 */

int main(int argc, char** argv) {
  int server_fd, new_socket, valread;
  struct sockaddr_in address;
  int opt = 1;
  int addrlen = sizeof(address);

  if (argc < 2) {
    std::cerr << "Usage: " << argv[0] << " <config_file>" << std::endl;
    return -1;
  }

  ServerConfig config = readServerConfig(argv[1]);

  std::cout << "Server Address: " << config.server_address << std::endl;
  std::cout << "Port: " << config.port << std::endl;
  std::cout << "Buffer: " << config.buffer_size << std::endl;
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
      char id_buffer[config.buffer_size];
      memset(id_buffer, 0, config.buffer_size);
      valread = read(new_socket, id_buffer, config.buffer_size);
      std::string client_id_str(id_buffer, valread);
      int client_id = std::stoi(client_id_str);
      std::cout << "Client id: " << client_id << std::endl;
      checkDirectory(config);
      std::fstream file = checkFileExistance(config, client_id, new_socket);

      while (1) {
        std::string recieved_file =
            checkFileStatus(file, new_socket, client_id, config);
        if (!recieved_file.empty()) {
          char readyBuffer[config.buffer_size] = {0};
          int messageLength = read(new_socket, readyBuffer, config.buffer_size);
          std::string clientMessage(readyBuffer, messageLength);
          std::cout << clientMessage << std::endl;
          if (clientMessage == "SENDING DATA") {
            std::string clientMessage(readyBuffer, messageLength);
            std::cout << "Client message: " << clientMessage << std::endl;
            sendFileData(config, client_id, new_socket, recieved_file, 0);
          } else if (clientMessage.substr(0, 15) == "RESUME DOWNLOAD") {
            std::istringstream ss(clientMessage.substr(
                16));  // Извлекаем оставшуюся часть сообщения
            std::string filename;
            size_t position;
            ss >> filename >> position;
            std::cout << "Resuming file transfer from: " << position
                      << " for file: " << filename << std::endl;
            //     sendFileData(config, client_id, new_socket, filename,
            //                  position);  // Функция отправки данных с учетом
            //                  позиции
          }
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

/**
 * @brief Проверяет и создает директорию, если она не существует.
 *
 * @param config Конфигурация сервера.
 */

void checkDirectory(ServerConfig& config) {
  struct stat st = {0};
  if (stat(config.directory.c_str(), &st) == -1) {
    std::cout << "Directory does not exist. Creating: " << config.directory
              << std::endl;
    mkdir(config.directory.c_str(), 0700);
  }
}

/**
 * @brief Проверяет наличие файла клиента и создает его, если не существует.
 *
 * @param config Конфигурация сервера.
 * @param client_id Идентификатор клиента.
 * @param new_socket Дескриптор сокета нового клиента.
 * @return Открытый файловый поток.
 */

std::fstream checkFileExistance(ServerConfig& config, int client_id,
                                int new_socket) {
  std::string file_path =
      config.directory + "/client_" + std::to_string(client_id) + ".txt";

  std::cout << "file_path: " << file_path << std::endl;
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

/**
 * @brief Читает значение размера файла из файла прогресса.
 *
 * @param file Открытый файловый поток для файла прогресса.
 * @param fileName Имя файла, для которого необходимо прочитать прогресс.
 * @return Размер файла в байтах, если найден; -1, если запись отсутствует.
 */

float readClientFile(std::fstream& file, const std::string& fileName) {
  file.clear();  // Очистка флагов состояния
  file.seekg(0, std::ios::beg);  // Перемещение указателя в начало файла
  std::string line;
  while (std::getline(file, line)) {
    std::istringstream iss(line);
    std::string key;
    float value;
    std::cout << fileName << std::endl;
    std::getline(iss, key, ':');
    std::cout << "key: " << key << " fileName: " << fileName << std::endl;
    if (key == fileName) {
      iss >> value;
      std::cout << "key: " << key << " value: " << value << std::endl;
      return value;  // Возвращаем значение, если нашли правильную строку
    }
  }
  return -1;  // Возвращаем -1, если файл не найден
}

/**
 * @brief Проверяет статус файла на сервере и обновляет файл прогресса клиента.
 *
 * @param file Открытый файловый поток для файла прогресса.
 * @param new_socket Сокет для общения с клиентом.
 * @param client_id Идентификатор клиента.
 * @param config Конфигурация сервера.
 * @return Имя файла, полученное от клиента.
 */

std::string checkFileStatus(std::fstream& file, int new_socket, int client_id,
                            ServerConfig& config) {
  char buffer[config.buffer_size] = {0};
  memset(buffer, 0, config.buffer_size);
  int valread = read(new_socket, buffer, config.buffer_size);
  if (valread > 0) {
    buffer[valread] = '\0';
    std::string clientFileName(buffer);
    std::cout << "Received file name: " << clientFileName << std::endl;

    std::string serverFilePath = "server_files/" + clientFileName;
    std::ifstream serverFile(serverFilePath);
    bool fileExistsOnServer = serverFile.good();
    serverFile.close();

    if (!fileExistsOnServer) {
      std::cerr << "File " << clientFileName << " not found on server."
                << std::endl;
      std::string response = "File not found";
      send(new_socket, response.c_str(), response.length(), 0);
      return "";  // Пропускаем текущий файл, возвращаем пустую строку
    }

    float file_size = readClientFile(file, clientFileName);
    if (file_size < 0) {  // Если файла нет в файле прогресса
      std::cout << "Adding new entry for: " << clientFileName << std::endl;
      file.clear();
      file.seekp(0, std::ios::end);
      file << clientFileName << ": " << 0 << std::endl;
      file_size = 0;
    }

    std::string response = "true " + std::to_string(file_size);
    send(new_socket, response.c_str(), response.length(), 0);
    return clientFileName;
  }
  return "";
}

/**
 * @brief Читает конфигурацию сервера из файла.
 *
 * @param filename Путь к файлу конфигурации.
 * @return Структура с настройками сервера.
 */

ServerConfig readServerConfig(const std::string& filename) {
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
      else if (key == "buffer_size")
        config.buffer_size = std::stoi(value.substr(1));
      else if (key == "directory")
        config.directory = value.substr(1);
    }
  }
  file.close();
  return config;
}

/**
 * @brief Отправляет данные файла клиенту, начиная с указанной позиции.
 *
 * @param config Конфигурация сервера.
 * @param client_id Идентификатор клиента.
 * @param new_socket Сокет для отправки данных.
 * @param file_name Имя файла, данные которого отправляются.
 * @param startPos Позиция в файле, с которой начинается отправка данных.
 */

void sendFileData(ServerConfig& config, int client_id, int new_socket,
                  const std::string& file_name, size_t startPos) {
  std::string file_path = "server_files/" + file_name;
  std::cout << "Sending file data: " << file_path << std::endl;

  std::ifstream file(file_path, std::ios::binary | std::ios::ate);
  if (!file.is_open()) {
    std::cerr << "Failed to open file: " << file_path << std::endl;
    return;
  }
  // file.seekg(startPos, std::ios::beg);
  size_t file_size = file.tellg();  // Получаем размер файла
  file.seekg(startPos,
             std::ios::beg);  // Возвращаемся к началу файла для чтения

  // Отправка размера файла клиенту
  std::string sizeMsg = std::to_string(file_size) + "\n";
  send(new_socket, sizeMsg.c_str(), sizeMsg.length(), 0);

  char buffer[config.buffer_size];
  size_t sent_bytes = startPos;
  while (!file.eof()) {
    file.read(buffer, sizeof(buffer));
    int bytes_to_send = file.gcount();
    send(new_socket, buffer, bytes_to_send, 0);
    sent_bytes += bytes_to_send;
    std::cout << "Total bytes sent for " << file_name << ": " << sent_bytes
              << std::endl;
  }

  file.close();
  updateProgressFile(
      config.directory + "/client_" + std::to_string(client_id) + ".txt",
      file_name, sent_bytes);
  const char* endOfData = "END_OF_DATA";
  send(new_socket, endOfData, strlen(endOfData), 0);

  std::cout << "Total bytes sent for " << file_name << ": " << sent_bytes
            << std::endl;
}

/**
 * @brief Обновляет файл прогресса, записывая текущее состояние передачи файлов.
 *
 * @param progressFilePath Путь к файлу прогресса.
 * @param fileName Имя файла.
 * @param sentBytes Количество отправленных байт.
 */

void updateProgressFile(const std::string& progressFilePath,
                        const std::string& fileName, size_t sentBytes) {
  std::map<std::string, size_t> progressData;
  std::ifstream inFile(progressFilePath);
  std::string line;

  // Читаем текущие данные прогресса
  while (getline(inFile, line)) {
    std::istringstream iss(line);
    std::string filename;
    size_t size;
    if (getline(iss, filename, ':')) {
      iss >> size;
      progressData[filename] = size;
    }
  }
  inFile.close();

  // Обновляем данные для текущего файла
  progressData[fileName] = sentBytes;

  // Перезаписываем файл с обновленной информацией
  std::ofstream outFile(progressFilePath);
  for (const auto& entry : progressData) {
    outFile << entry.first << ": " << entry.second << std::endl;
  }
  outFile.close();
}