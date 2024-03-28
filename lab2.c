#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#define MAX_MANIPULATORS 10
#define MOVEMENT_MESSAGE_SIZE 100

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
  int width;
  int height;
  int count;
  int controlled_manip;
} Field;

Field field;

pthread_mutex_t lock;
unsigned int rand_state = 0;

void *manipulator_routine(void *arg);
void create_manipulator(int x, int y, int speed, char direction, int id);
void *visualizer_routine(void *arg);
void *controller_routine(void *arg);
void print_field();
char get_input();
int read_number();

void *manipulator_routine(void *arg) {
  Manipulator *manip = (Manipulator *)arg;
  int oldX, oldY;

  while (manip->active) {
    pthread_mutex_lock(&lock);
    oldX = manip->x;
    oldY = manip->y;

    switch (manip->direction) {
      case 'W':
        if (manip->y > 0) manip->y--;
        break;
      case 'S':
        if (manip->y < field.height - 1) manip->y++;
        break;
      case 'A':
        if (manip->x > 0) manip->x--;
        break;
      case 'D':
        if (manip->x < field.width - 1) manip->x++;
        break;
    }
    Manipulator *ctrl_manip = &field.manipulators[field.controlled_manip];
    if (oldX != ctrl_manip->x || oldY != ctrl_manip->y) {
      printf("\033[%d;0H", field.height + 2);  // Перемещаем курсор ниже поля
      printf("\033[K");                        // Очистить строку
      printf("\033[K");
      printf("Текущий манипулятор: %d\n", ctrl_manip->id);
      printf("Перемещение манипулятора из (%d, %d) в (% d, % d)\n ",
             oldX,
             oldY, ctrl_manip->x, ctrl_manip->y);
      printf("Передать управление манипулятору C");
    }

    pthread_mutex_unlock(&lock);
    sleep(manip->speed);
  }
  return NULL;
}

void create_manipulator(int x, int y, int speed, char direction, int id) {
  pthread_mutex_lock(&lock);
  if (field.count >= MAX_MANIPULATORS) {
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
      printf("%c ", symbol);
    }
    printf("\n");
  }
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
  while (1) {
    input = get_input();
    if (input == 'c' || input == 'C') {
      printf("\n\n\nВыберите манипулятор для управления (%d - %d): ", 0,
             field.count - 1);
      int num = read_number();
      if (num >= 0 && num < field.count) field.controlled_manip = num;
    } else if (input == 'e' || input == 'E') {
      if (field.count < MAX_MANIPULATORS) {
        int x = rand_r(&rand_state) % field.width;
        int y = rand_r(&rand_state) % field.height;
        pthread_mutex_unlock(&lock);
        create_manipulator(x, y, 1, ' ', field.count);
      }
    } else {
      if (field.count > 0) {
        Manipulator *ctrl_manip = &field.manipulators[field.controlled_manip];
        if (input == 'w' || input == 'W')
          ctrl_manip->direction = 'W';
        else if (input == 's' || input == 'S')
          ctrl_manip->direction = 'S';
        else if (input == 'a' || input == 'A')
          ctrl_manip->direction = 'A';
        else if (input == 'd' || input == 'D')
          ctrl_manip->direction = 'D';
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
  field.width = 10;  // Пример размера поля
  field.height = 10;
  system("clear");
  rand_state = time(NULL);
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
