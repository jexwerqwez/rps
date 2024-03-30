// Подключаем необходимые библиотеки
#include <ctype.h>    // Для функции isdigit
#include <pthread.h>  // Для работы с потоками
#include <stdbool.h>  // Для использования типа bool (истина/ложь)
#include <stdio.h>  // Стандартная библиотека ввода-вывода
#include <stdlib.h>  // Стандартная библиотека языка С
#include <string.h>  // Для работы со строками
#include <termios.h>  // Для работы с терминальным вводом
#include <time.h>  // Для работы со временем и рандомизации
#include <unistd.h>  // Для различных системных вызовов

// Определяем константы для размеров поля и максимального количества
// манипуляторов
#define N 5
#define M 5
#define MAX_MANIPULATORS (N * M - 1)  // Максимальное количество манипуляторов
#define MOVEMENT_MESSAGE_SIZE 100  // Размер сообщения о движении
#define MAX_ITEMS 5  // Максимальное количество элементов

// Определение структуры для предметов на поле
typedef struct {
  int x;  // Позиция по оси X
  int y;  // Позиция по оси Y
} Item;

// Определение структуры для манипулятора
typedef struct {
  bool active;          // Статус активности
  int x, y;             // Позиции манипулятора
  int speed;            // Скорость перемещения
  char direction;       // Направление движения
  pthread_t thread_id;  // Идентификатор потока
  int id;               // Идентификатор манипулятора
} Manipulator;

// Определение структуры для поля
typedef struct {
  Manipulator manipulators[MAX_MANIPULATORS];  // Массив манипуляторов
  Item items[MAX_ITEMS];  // Массив предметов
  int width, height;      // Размеры поля
  int count;  // Количество активных манипуляторов
  int controlled_manip;  // ID управляемого манипулятора
  int items_count;  // Количество предметов на поле
} Field;

// Инициализация глобальных переменных
Field field;           // Игровое поле
pthread_mutex_t lock;  // Мьютекс для синхронизации потоков
unsigned int rand_state = 0;  // Состояние генератора случайных чисел

// Прототипы функций
void *manipulator_routine(void *arg);
void create_manipulator(int x, int y, int speed, char direction, int id);
void deactivate_manipulator(int id);
void *visualizer_routine();
void *controller_routine();
void print_field();
char get_input();
int read_number();
void initialize_items();
bool pick_up_item(Manipulator *manip);

// Функция для инициализации предметов на поле
void initialize_items() {
  for (int i = 0; i < MAX_ITEMS; i++) {
    // Генерируем случайные координаты для каждого предмета
    int x = rand_r(&rand_state) % field.width;
    int y = rand_r(&rand_state) % field.height;
    // Устанавливаем координаты предмета
    field.items[i].x = x;
    field.items[i].y = y;
  }
  // Устанавливаем количество предметов на поле
  field.items_count = MAX_ITEMS;
}

// Функция для поднятия предмета манипулятором
bool pick_up_item(Manipulator *manip) {
  // Перебираем все предметы на поле
  for (int i = 0; i < field.items_count; i++) {
    // Проверяем, находится ли манипулятор в той же позиции, что и предмет
    if (field.items[i].x == manip->x && field.items[i].y == manip->y) {
      // Выводим сообщение о поднятии предмета
      printf("Манипулятор %d поднял предмет на позиции (%d, %d)\n", manip->id,
             manip->x, manip->y);
      // Сдвигаем предметы, удаляя поднятый
      for (int j = i; j < field.items_count - 1; j++) {
        field.items[j] = field.items[j + 1];
      }
      // Уменьшаем количество предметов на поле
      field.items_count--;
      return true;  // Возвращаем true, так как предмет был поднят
    }
  }
  return false;  // Возвращаем false, если предмет не был поднят
}

// Функция для проверки столкновения манипулятора с другими объектами
bool check_collision(int x, int y) {
  // Перебираем всех манипуляторов на поле
  for (int i = 0; i < field.count; i++) {
    // Проверяем, не занимает ли какой-либо манипулятор ту же позицию
    if (field.manipulators[i].active && field.manipulators[i].x == x &&
        field.manipulators[i].y == y) {
      return true;  // Возвращаем true, если обнаружено столкновение
    }
  }
  return false;  // Возвращаем false, если столкновения нет
}

// Главный цикл работы манипулятора
void *manipulator_routine(void *arg) {
  Manipulator *manip =
      (Manipulator *)arg;  // Преобразуем аргумент в тип Manipulator
  int oldX, oldY, newX,
      newY;  // Переменные для хранения текущих и новых координат

  // Цикл работы манипулятора
  while (manip->active) {
    // Блокируем мьютекс для синхронизации доступа к общим данным
    pthread_mutex_lock(&lock);

    // Запоминаем текущие координаты манипулятора
    oldX = manip->x;
    oldY = manip->y;
    newX = oldX;
    newY = oldY;

    // Определяем новые координаты в зависимости от направления движения
    switch (manip->direction) {
      case 'W':  // Вверх
        newY = (newY > 0) ? newY - 1 : newY;
        break;
      case 'S':  // Вниз
        newY = (newY < field.height - 1) ? newY + 1 : newY;
        break;
      case 'A':  // Влево
        newX = (newX > 0) ? newX - 1 : newX;
        break;
      case 'D':  // Вправо
        newX = (newX < field.width - 1) ? newX + 1 : newX;
        break;
    }

    bool itemPickedUp = false;  // Флаг для проверки поднятия предмета

    // Проверяем, можно ли переместиться на новую позицию
    if (!check_collision(newX, newY)) {
      // Обновляем координаты манипулятора
      manip->x = newX;
      manip->y = newY;
      // Проверяем и поднимаем предмет, если он есть
      itemPickedUp = pick_up_item(manip);
    } else {
      // Меняем направление движения при обнаружении столкновения
      switch (manip->direction) {
        case 'W':
          manip->direction = 'S';
          break;
        case 'S':
          manip->direction = 'W';
          break;
        case 'A':
          manip->direction = 'D';
          break;
        case 'D':
          manip->direction = 'A';
          break;
      }
    }

    // Выводим информацию о перемещении, если предмет не был поднят
    if (!itemPickedUp && (oldX != manip->x || oldY != manip->y)) {
      printf("Манипулятор %d переместился из (%d, %d) в (%d, %d)\n", manip->id,
             oldX, oldY, manip->x, manip->y);
    }

    // Разблокируем мьютекс
    pthread_mutex_unlock(&lock);
    // Задержка, имитирующая скорость движения манипулятора
    sleep(manip->speed);
  }
  return NULL;
}

// Функция для создания манипулятора
void create_manipulator(int x, int y, int speed, char direction, int id) {
  // Блокируем мьютекс для синхронизации
  pthread_mutex_lock(&lock);

  // Проверяем, не превышено ли максимальное количество манипуляторов и нет ли
  // столкновения
  if (field.count >= MAX_MANIPULATORS || check_collision(x, y)) {
    // Если условие верно, разблокируем мьютекс и выходим из функции
    pthread_mutex_unlock(&lock);
    return;
  }

  // Инициализируем новый манипулятор
  Manipulator *manip = &field.manipulators[field.count++];
  manip->x = x;
  manip->y = y;
  manip->speed = speed;
  manip->direction = direction;
  manip->active = true;
  manip->id = id;

  // Создаем новый поток для манипулятора
  pthread_create(&manip->thread_id, NULL, manipulator_routine, manip);

  // Устанавливаем первого созданного манипулятора как управляемого
  if (field.count == 1) field.controlled_manip = 0;

  // Разблокируем мьютекс
  pthread_mutex_unlock(&lock);
}

// Функция для деактивации и удаления манипулятора
void deactivate_manipulator(int id) {
  // Блокируем мьютекс для синхронизации
  pthread_mutex_lock(&lock);

  // Перебираем все манипуляторы в поисках нужного
  for (int i = 0; i < field.count; i++) {
    // Проверяем ID и активность манипулятора
    if (field.manipulators[i].id == id && field.manipulators[i].active) {
      // Деактивируем манипулятор
      field.manipulators[i].active = false;
      // Выводим сообщение об удалении
      printf("\n\n\nМанипулятор %d удалён.\n", id);
      // Отменяем поток манипулятора
      pthread_cancel(field.manipulators[i].thread_id);
      // Ожидаем завершения потока
      pthread_join(field.manipulators[i].thread_id, NULL);

      // Удаляем манипулятор из массива, сдвигая оставшиеся элементы
      for (int j = i; j < field.count - 1; j++) {
        field.manipulators[j] = field.manipulators[j + 1];
      }
      // Уменьшаем количество манипуляторов
      field.count--;
      break;  // Выходим из цикла после удаления
    }
  }

  // Разблокируем мьютекс
  pthread_mutex_unlock(&lock);
}

// Функция потока для визуализации поля
void *visualizer_routine() {
  while (1) {
    // Блокируем мьютекс для синхронизации
    pthread_mutex_lock(&lock);
    // Выводим текущее состояние поля
    print_field();
    // Разблокируем мьютекс
    pthread_mutex_unlock(&lock);
    // Ожидание перед следующим обновлением
    usleep(50000);
  }
  return NULL;
}

// Функция для отображения поля и управляющих инструкций
void print_field() {
  // Перемещаем курсор в начало консоли
  printf("\033[0;0H");
  // Выводим управляющие инструкции
  printf(
      "Управление:\nE - добавить манипулятор;\tR - удалить манипулятор;\nC - "
      "сменить манипулятор;\tV - изменить скорость\n\n");
  // Выводим границу поля
  printf(
      "==================================================================\n");
  // Перебираем все ячейки поля
  for (int y = 0; y < field.height; y++) {
    for (int x = 0; x < field.width; x++) {
      char symbol = '.';  // Символ пустой ячейки
      // Перебираем всех манипуляторов для отображения на поле
      for (int i = 0; i < field.count; i++) {
        if (field.manipulators[i].active && field.manipulators[i].x == x &&
            field.manipulators[i].y == y) {
          symbol = 'M';  // Символ манипулятора
          break;
        }
      }
      // Перебираем все предметы для отображения на поле
      for (int i = 0; i < field.items_count; i++) {
        if (field.items[i].x == x && field.items[i].y == y) {
          symbol = '#';  // Символ предмета
          break;
        }
      }
      // Выводим символ ячейки
      printf("%c ", symbol);
    }
    // Переход на новую строку после завершения строки поля
    printf("\n");
  }
  // Выводим нижнюю границу поля
  printf(
      "==================================================================\n");
}

// Функция для считывания числа из ввода пользователя
int read_number() {
  char number_str[10];  // Строка для хранения введенного числа
  bool is_number = true;  // Флаг проверки, что введено число

  // Считываем строку
  fgets(number_str, sizeof(number_str), stdin);
  // Проверяем, состоит ли строка из цифр
  for (size_t i = 0; i < strlen(number_str) - 1; i++) {
    if (!isdigit(number_str[i])) {
      is_number = false;
      break;
    }
  }

  // Возвращаем число или -1, если введены не только цифры
  return is_number ? atoi(number_str) : -1;
}

// Функция для получения одного символа ввода без необходимости нажимать Enter
char get_input() {
  struct termios oldt, newt;  // Структуры для управления терминалом
  char ch;  // Символ для ввода

  // Получаем текущие настройки терминала
  tcgetattr(STDIN_FILENO, &oldt);
  newt = oldt;
  // Отключаем канонический режим и эхо
  newt.c_lflag &= ~(ICANON | ECHO);
  // Устанавливаем новые настройки терминала
  tcsetattr(STDIN_FILENO, TCSANOW, &newt);
  // Читаем символ
  ch = getchar();
  // Восстанавливаем старые настройки терминала
  tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
  return ch;
}

// Главный цикл управления манипуляторами
void *controller_routine() {
  char input;  // Символ для ввода команды
  Manipulator *ctrl_manip;  // Указатель на управляемого манипулятора
  int oldX, oldY;  // Старые координаты манипулятора
  bool menu_visible = true;  // Флаг видимости меню

  // Основной цикл обработки ввода
  while (1) {
    input = get_input();  // Получаем ввод от пользователя

    // Определяем, нужно ли отобразить меню
    if (input == 'c' || input == 'C') {
      menu_visible = true;
    } else if (input >= '0' && input <= '9') {
      menu_visible = false;
    }

    // Блокируем мьютекс для синхронизации
    pthread_mutex_lock(&lock);
    if (!menu_visible) {
      printf("Управляемый манипулятор: %d\n", field.controlled_manip);
    }
    // Выводим текущее состояние поля
    print_field();
    // Разблокируем мьютекс
    pthread_mutex_unlock(&lock);

    // Если меню не активно, ждем следующего ввода
    if (!menu_visible) {
      usleep(100000);
      continue;
    }

    // Обрабатываем команды пользователя
    if (input == 'r' || input == 'R') {
      // Удаление манипулятора
      if (field.count > 0) {
        printf("\nВыберите манипулятор для удаления (%d - %d): ", 0,
               field.count - 1);
        int num = read_number();
        if (num == -1 || num < 0 || num >= field.count) {
          printf(
              "\nНеверный ввод. Пожалуйста, введите корректный номер "
              "манипулятора.\n");
        } else {
          deactivate_manipulator(num);
          // Если удаляем управляемый манипулятор, переключаем управление
          if (field.controlled_manip == num)
            field.controlled_manip = (num == 0) ? 1 : num - 1;
        }
      }
    } else if (input == 'c' || input == 'C') {
      // Смена управляемого манипулятора
      printf("\nВыберите манипулятор для управления (%d - %d): ", 0,
             field.count - 1);
      int num = read_number();
      if (num == -1 || num < 0 || num >= field.count) {
        printf(
            "\nНеверный ввод. Пожалуйста, введите корректный номер "
            "манипулятора.\n");
      } else {
        field.controlled_manip = num;
        printf("\n\n\nКонтроль передан манипулятору: %d\n", num);
      }
    } else if (input == 'e' || input == 'E') {
      // Добавление нового манипулятора
      if (field.count < MAX_MANIPULATORS) {
        int x = rand_r(&rand_state) % field.width;
        int y = rand_r(&rand_state) % field.height;
        create_manipulator(x, y, 1, ' ', field.count);
      }
    } else if (input == 'v' || input == 'V') {
      // Изменение скорости управляемого манипулятора
      ctrl_manip = &field.manipulators[field.controlled_manip];
      printf("\nВведите новую скорость для манипулятора %d: ", ctrl_manip->id);
      int new_speed = read_number();
      if (new_speed > 0) {
        ctrl_manip->speed = new_speed;
        printf("\nСкорость манипулятора %d изменена на %d.\n", ctrl_manip->id,
               new_speed);
      }
    } else {
      // Обработка управления манипулятором через клавиши WASD
      if (field.count > 0) {
        ctrl_manip = &field.manipulators[field.controlled_manip];
        oldX = ctrl_manip->x;
        oldY = ctrl_manip->y;
        switch (input) {
          case 'w':
          case 'W':
            ctrl_manip->direction = 'W';
            break;
          case 's':
          case 'S':
            ctrl_manip->direction = 'S';
            break;
          case 'a':
          case 'A':
            ctrl_manip->direction = 'A';
            break;
          case 'd':
          case 'D':
            ctrl_manip->direction = 'D';
            break;
        }
        printf("Манипулятор %d переместился из (%d, %d) в (%d, %d)\n",
               ctrl_manip->id, oldX, oldY, ctrl_manip->x, ctrl_manip->y);
      }
    }
    // Разблокируем мьютекс
    pthread_mutex_unlock(&lock);
    // Пауза для следующего ввода
    usleep(100000);
  }
  return NULL;
}

// Главная функция программы
int main() {
  pthread_t visualizer_thread,
      controller_thread;  // Потоки для визуализации и управления

  srand(time(NULL));  // Инициализируем генератор случайных чисел
  field.width = N;   // Устанавливаем ширину поля
  field.height = M;  // Устанавливаем высоту поля
  system("clear");   // Очищаем консоль
  rand_state = time(NULL);  // Инициализируем состояние для rand_r
  initialize_items();  // Инициализируем предметы на поле

  // Создаем начальный манипулятор
  int startX = rand_r(&rand_state) % field.width;
  int startY = rand_r(&rand_state) % field.height;
  create_manipulator(startX, startY, 1, ' ', 0);

  // Инициализируем мьютекс
  pthread_mutex_init(&lock, NULL);
  // Создаем потоки для визуализации и управления
  pthread_create(&visualizer_thread, NULL, visualizer_routine, NULL);
  pthread_create(&controller_thread, NULL, controller_routine, NULL);

  // Ожидаем завершения потоков
  pthread_join(visualizer_thread, NULL);
  pthread_join(controller_thread, NULL);

  // Уничтожаем мьютекс
  pthread_mutex_destroy(&lock);

  return 0;  // Возвращаем 0 в качестве знака успешного выполнения программы
}
