#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <semaphore.h>
#include <signal.h>
#include <time.h>

#define HOTEL_SIZE 10 // Количество номеров в гостинице
#define CUSTOMER_COUNT 15 // Количество клиентов

// Структура, описывающая гостиницу
typedef struct {
    int rooms[HOTEL_SIZE]; // Массив номеров
    int queue[CUSTOMER_COUNT]; // Очередь ожидающих клиентов
    int queue_size; // Размер очереди
} Hotel;

Hotel *hotel; // Указатель на структуру гостиницы
sem_t *sem; // Семафор

// Функция для очистки ресурсов
void cleanup() {
    printf("Очистка...\n");
    munmap(hotel, sizeof(Hotel)); // Освобождение разделяемой памяти
    sem_unlink("/hotel_sem"); // Удаление семафора
}

// Обработчик сигнала прерывания
void handle_sigint(int sig) {
    printf("Получен сигнал прерывания. Выход...\n");
    cleanup(); // Очистка ресурсов
    exit(0); // Завершение программы
}

// Функция, моделирующая работу администратора гостиницы
void admin() {
    while (1) {
        sem_wait(sem); // Ожидание семафора
        printf("Администратор проверяет номера...\n");
        // Проверка номеров и назначение ожидающим клиентам
        for (int i = 0; i < HOTEL_SIZE; i++) {
            if (hotel->rooms[i] == 0 && hotel->queue_size > 0) {
                // Назначение номера первому клиенту в очереди
                hotel->rooms[i] = hotel->queue[0];
                printf("Клиент %d заселился в номер %d.\n", hotel->queue[0], i);
                // Удаление первого клиента из очереди
                for (int j = 0; j < hotel->queue_size - 1; j++) {
                    hotel->queue[j] = hotel->queue[j + 1];
                }
                hotel->queue_size--;
            }
        }
        sem_post(sem); // Освобождение семафора
        sleep(2); // Задержка
    }
}

// Функция, моделирующая поведение клиента
void customer(int id) {
    srand(time(NULL) + id); // Инициализация генератора случайных чисел
    int stay_duration = (rand() % 2 + 1) * 2; // Случайный выбор продолжительности пребывания (2 или 4 секунды)
    printf("Клиент %d будет жить в номере %d секунды.\n", id, stay_duration);

    while (1) {
        sem_wait(sem); // Ожидание семафора
        // Проверка, находится ли клиент в очереди
        int in_queue = 0;
        for (int i = 0; i < hotel->queue_size; i++) {
            if (hotel->queue[i] == id) {
                in_queue = 1;
                break;
            }
        }
        // Если клиент не в очереди, запрос номера
        if (!in_queue) {
            printf("Клиент %d запрашивает номер...\n", id);
            // Запрос номера или ожидание
            int all_rooms_occupied = 1;
            for (int i = 0; i < HOTEL_SIZE; i++) {
                if (hotel->rooms[i] == 0) {
                    hotel->rooms[i] = id; // Назначение номера клиенту
                    printf("Клиент %d заселился в номер %d.\n", id, i);
                    all_rooms_occupied = 0;
                    break;
                }
            }
            if (all_rooms_occupied) {
                printf("Все номера заняты. Клиент %d ждет свободный номер...\n", id);
                hotel->queue[hotel->queue_size++] = id;
                printf("Клиент %d теперь на позиции %d в очереди.\n", id, hotel->queue_size);
            }
        }
        sem_post(sem); // Освобождение семафора
        sleep(stay_duration); // Сон на время пребывания

        // После пребывания, освобождение номера
        sem_wait(sem); // Ожидание семафора
        for (int i = 0; i < HOTEL_SIZE; i++) {
            if (hotel->rooms[i] == id) {
                hotel->rooms[i] = 0; // Освобождение номера
                printf("Клиент %d освободил номер %d.\n", id, i);
                // После освобождения номера, добавление клиента в конец очереди
                hotel->queue[hotel->queue_size++] = id;
                printf("Клиент %d теперь на позиции %d в очереди.\n", id, hotel->queue_size);
                break;
            }
        }
        sem_post(sem); // Освобождение семафора
    }
}

int main() {
    signal(SIGINT, handle_sigint); // Установка обработчика сигнала прерывания
    atexit(cleanup); // Установка функции для очистки ресурсов при завершении программы

    // Создание разделяемой памяти для структуры гостиницы
    hotel = mmap(NULL, sizeof(Hotel), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    // Создание семафора
    sem = sem_open("/hotel_sem", O_CREAT, 0644, 1);

    printf("Запуск процесса администратора...\n");
    if (fork() == 0) {
        admin(); // Запуск функции администратора в дочернем процессе
        exit(0);
    }

    for (int i = 0; i < CUSTOMER_COUNT; i++) {
        printf("Запуск процесса клиента %d...\n", i);
        if (fork() == 0) {
            customer(i); // Запуск функции клиента в дочернем процессе
            exit(0);
        }
        sleep(1); // Задержка
    }

    printf("Главный процесс входит в состояние ожидания...\n");
    while (1) pause(); // Ожидание сигналов

    return 0;
}