#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#define N 5
#define M 5
#define MAX_MANIPULATORS (N * M - 1)
#define MOVEMENT_MESSAGE_SIZE 100
#define MAX_ITEMS 5

typedef struct {
  int x;
  int y;
} Item;

typedef struct {
  bool active;
  int x;
  int y;
  int speed;
  char direction;
  pthread_t thread_id;
  int id;
} Manipulator;

typedef struct {
  Manipulator manipulators[MAX_MANIPULATORS];
  Item items[MAX_ITEMS];
  int width;
  int height;
  int count;
  int controlled_manip;
  int items_count;
} Field;

Field field;

pthread_mutex_t lock;
unsigned int rand_state = 0;

void *manipulator_routine(void *arg);
void create_manipulator(int x, int y, int speed, char direction, int id);
void deactivate_manipulator(int id);
void *visualizer_routine(void *arg);
void *controller_routine(void *arg);
void print_field();
char get_input();
int read_number();
void initialize_items();
void pick_up_item(Manipulator *manip);

void initialize_items() {
  for (int i = 0; i < MAX_ITEMS; i++) {
    int x = rand_r(&rand_state) % field.width;
    int y = rand_r(&rand_state) % field.height;
    field.items[i].x = x;
    field.items[i].y = y;
  }
  field.items_count = MAX_ITEMS;
}

void pick_up_item(Manipulator *manip) {
  for (int i = 0; i < field.items_count; i++) {
    if (field.items[i].x == manip->x && field.items[i].y == manip->y) {
      printf("Манипулятор %d поднял предмет на позиции (%d, %d)", manip->id,
             manip->x, manip->y);
      // Сдвигаем все последующие элементы на одну позицию влево
      for (int j = i; j < field.items_count - 1; j++) {
        field.items[j] = field.items[j + 1];
      }
      field.items_count--;  // Уменьшаем количество предметов
      break;  // Выходим из цикла после обработки первого найденного предмета
    }
  }
}

bool check_collision(int x, int y) {
  for (int i = 0; i < field.count; i++) {
    if (field.manipulators[i].active && field.manipulators[i].x == x &&
        field.manipulators[i].y == y) {
      return true;
    }
  }
  return false;
}

void *manipulator_routine(void *arg) {
  Manipulator *manip = (Manipulator *)arg;
  int oldX, oldY, newX, newY;

  while (manip->active) {
    pthread_mutex_lock(&lock);
    oldX = manip->x;
    oldY = manip->y;
    newX = oldX;
    newY = oldY;

    // Определяем новые координаты в зависимости от направления
    switch (manip->direction) {
      case 'W':
        newY = (newY > 0) ? newY - 1 : newY;
        break;
      case 'S':
        newY = (newY < field.height - 1) ? newY + 1 : newY;
        break;
      case 'A':
        newX = (newX > 0) ? newX - 1 : newX;
        break;
      case 'D':
        newX = (newX < field.width - 1) ? newX + 1 : newX;
        break;
    }

    // Проверяем, можно ли переместиться на новую позицию
    if (!check_collision(newX, newY)) {
      manip->x = newX;
      manip->y = newY;
      pick_up_item(manip);  // Проверяем и поднимаем предмет, если он есть
    } else {
      // Меняем направление при обнаружении столкновения
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

    if (oldX != manip->x || oldY != manip->y) {
      printf("Манипулятор %d переместился из (%d, %d) в (%d, %d)", manip->id,
             oldX, oldY, manip->x, manip->y);
    }

    pthread_mutex_unlock(&lock);
    sleep(manip->speed);
  }
  return NULL;
}

void create_manipulator(int x, int y, int speed, char direction, int id) {
  pthread_mutex_lock(&lock);
  if (field.count >= MAX_MANIPULATORS || check_collision(x, y)) {
    pthread_mutex_unlock(&lock);
    return;
  }
  Manipulator *manip = &field.manipulators[field.count++];
  manip->x = x;
  manip->y = y;
  manip->speed = speed;
  manip->direction = direction;
  manip->active = true;
  manip->id = id;
  pthread_create(&manip->thread_id, NULL, manipulator_routine, manip);
  if (field.count == 1) field.controlled_manip = 0;
  pthread_mutex_unlock(&lock);
}

void deactivate_manipulator(int id) {
  pthread_mutex_lock(&lock);
  for (int i = 0; i < field.count; i++) {
    if (field.manipulators[i].id == id && field.manipulators[i].active) {
      field.manipulators[i].active = false;
      printf("\n\n\nМанипулятор %d удалён.\n", id);
      pthread_cancel(
          field.manipulators[i].thread_id);  // Отмена потока манипулятора
      pthread_join(field.manipulators[i].thread_id,
                   NULL);  // Ожидание завершения потока

      // Удаление манипулятора из массива
      for (int j = i; j < field.count - 1; j++) {
        field.manipulators[j] = field.manipulators[j + 1];
      }
      field.count--;

      break;
    }
  }
  pthread_mutex_unlock(&lock);
}

void *visualizer_routine(void *arg) {
  while (1) {
    pthread_mutex_lock(&lock);
    print_field();
    pthread_mutex_unlock(&lock);
    usleep(50000);
  }
  return NULL;
}

void print_field() {
  printf("\033[0;0H");
  printf(
      "Управление:\nE - добавить манипулятор;\tR - удалить манипулятор;\nC - "
      "сменить манипулятор;\tV - изменить скорость\n\n");
  printf(
      "==================================================================\n");
  for (int y = 0; y < field.height; y++) {
    for (int x = 0; x < field.width; x++) {
      char symbol = '.';
      for (int i = 0; i < field.count; i++) {
        if (field.manipulators[i].active && field.manipulators[i].x == x &&
            field.manipulators[i].y == y) {
          symbol = 'M';
          break;
        }
      }
      for (int i = 0; i < field.items_count; i++) {
        if (field.items[i].x == x && field.items[i].y == y) {
          symbol = '#';
          break;
        }
      }
      printf("%c ", symbol);
    }
    printf("\n");
  }
  printf(
      "==================================================================\n");
}

int read_number() {
  char number_str[10];
  fgets(number_str, sizeof(number_str), stdin);
  return atoi(number_str);
}

char get_input() {
  struct termios oldt, newt;
  char ch;
  tcgetattr(STDIN_FILENO, &oldt);
  newt = oldt;
  newt.c_lflag &= ~(ICANON | ECHO);
  tcsetattr(STDIN_FILENO, TCSANOW, &newt);
  ch = getchar();
  tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
  return ch;
}

void *controller_routine(void *arg) {
  char input;
  Manipulator *ctrl_manip;
  int oldX, oldY;
  bool menu_visible = true;

  while (1) {
    input = get_input();

    // Проверка для отображения/скрытия меню
    if (input == 'c' || input == 'C') {
      menu_visible = true;
    } else if (input >= '0' && input <= '9') {
      menu_visible = false;
    }

    // Очистка экрана и вывод информации о перемещениях
    pthread_mutex_lock(&lock);
    if (!menu_visible) {
      printf("Управляемый манипулятор: %d\n", field.controlled_manip);
    }
    print_field();
    pthread_mutex_unlock(&lock);

    if (!menu_visible) {
      usleep(100000);
      continue;
    }

    // Обработка пользовательского ввода
    if (input == 'r' || input == 'R') {
      if (field.count > 0) {
        printf("\nВыберите манипулятор для удаления (%d - %d): ", 0,
               field.count - 1);
        int num = read_number();
        if (num >= 0 && num < field.count) {
          deactivate_manipulator(num);
          if (field.controlled_manip == num)
            field.controlled_manip = (num == 0) ? 1 : num - 1;
        }
      }
    } else if (input == 'c' || input == 'C') {
      printf("\nВыберите манипулятор для управления (%d - %d): ", 0,
             field.count - 1);
      int num = read_number();
      if (num >= 0 && num < field.count) {
        field.controlled_manip = num;
        printf("\nКонтроль передан манипулятору: %d\n", num);
      }
    } else if (input == 'e' || input == 'E') {
      if (field.count < MAX_MANIPULATORS) {
        int x = rand_r(&rand_state) % field.width;
        int y = rand_r(&rand_state) % field.height;
        pthread_mutex_unlock(&lock);
        create_manipulator(x, y, 1, ' ', field.count);
      }
    } else if (input == 'v' || input == 'V') {
      ctrl_manip = &field.manipulators[field.controlled_manip];
      printf("\nВведите новую скорость для манипулятора %d: ", ctrl_manip->id);
      int new_speed = read_number();
      if (new_speed > 0) {
        ctrl_manip->speed = new_speed;
        printf("\nСкорость манипулятора %d изменена на %d.\n", ctrl_manip->id,
               new_speed);
      }
    } else {
      if (field.count > 0) {
        ctrl_manip = &field.manipulators[field.controlled_manip];
        oldX = ctrl_manip->x;
        oldY = ctrl_manip->y;
        if (input == 'w' || input == 'W')
          ctrl_manip->direction = 'W';
        else if (input == 's' || input == 'S')
          ctrl_manip->direction = 'S';
        else if (input == 'a' || input == 'A')
          ctrl_manip->direction = 'A';
        else if (input == 'd' || input == 'D')
          ctrl_manip->direction = 'D';
        printf("Манипулятор %d переместился из (%d, %d) в (%d, %d)\n",
               ctrl_manip->id, oldX, oldY, ctrl_manip->x, ctrl_manip->y);
      }
    }
    pthread_mutex_unlock(&lock);
    usleep(100000);
  }
  return NULL;
}

int main() {
  pthread_t visualizer_thread, controller_thread;

  srand(time(NULL));  // Инициализация генератора случайных чисел
  field.width = N;
  field.height = M;
  system("clear");
  rand_state = time(NULL);
  initialize_items();
  // Создание манипулятора в случайной позиции
  int startX = rand_r(&rand_state) % field.width;
  int startY = rand_r(&rand_state) % field.height;
  create_manipulator(startX, startY, 1, ' ', 0);

  pthread_mutex_init(&lock, NULL);
  pthread_create(&visualizer_thread, NULL, visualizer_routine, NULL);
  pthread_create(&controller_thread, NULL, controller_routine, NULL);

  // Присоединение потоков перед завершением программы
  pthread_join(visualizer_thread, NULL);
  pthread_join(controller_thread, NULL);

  pthread_mutex_destroy(&lock);
  return 0;
}
