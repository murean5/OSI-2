#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/sem.h>
#include <sys/ipc.h>
#include <sys/shm.h>
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

int sem_id; // ID семафора
int shm_id; // ID разделяемой памяти
Hotel *hotel; // Указатель на структуру гостиницы

// Функция для управления семафором
void sem_op(int op) {
    struct sembuf sops = {0, op, 0};
    semop(sem_id, &sops, 1);
}

// Функция для очистки ресурсов
void cleanup() {
    printf("Очистка...\n");
    semctl(sem_id, 0, IPC_RMID); // Уничтожение семафора
    shmdt(hotel); // Отсоединение разделяемой памяти
    shmctl(shm_id, IPC_RMID, NULL); // Удаление разделяемой памяти
}

// Обработчик сигнала прерывания
void handle_sigint(int sig) {
    cleanup(); // Очистка ресурсов
    exit(0); // Завершение программы
}

// Функция, моделирующая поведение клиента
void customer(int id) {
    srand(time(NULL) + id); // Инициализация генератора случайных чисел
    int stay_duration = (rand() % 2 + 1) * 2; // Случайный выбор продолжительности пребывания (2 или 4 секунды)
    printf("Клиент %d будет жить в номере %d секунды.\n", id, stay_duration);

    while (1) {
        sem_op(-1); // Ожидание семафора
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
        sem_op(1); // Освобождение семафора
        sleep(stay_duration); // Сон на время пребывания

        // После пребывания, освобождение номера
        sem_op(-1); // Ожидание семафора
        for (int i = 0; i < HOTEL_SIZE; i++) {
            if (hotel->rooms[i] == id) {
                hotel->rooms[i] = 0; // Освобождение номера
                printf("Клиент %d освободил номер %d.\n", id, i);
                break;
            }
        }
        // Проверка, есть ли клиенты в очереди
        if (hotel->queue_size > 0) {
            // Заселение первого в очереди клиента в только что освободившийся номер
            for (int i = 0; i < HOTEL_SIZE; i++) {
                if (hotel->rooms[i] == 0) {
                    hotel->rooms[i] = hotel->queue[0];
                    printf("Клиент %d заселился в номер %d.\n", hotel->queue[0], i);
                    // Удаление клиента из очереди
                    for (int j = 0; j < hotel->queue_size - 1; j++) {
                        hotel->queue[j] = hotel->queue[j + 1];
                    }
                    hotel->queue_size--;
                    break;
                }
            }
        }
        sem_op(1); // Освобождение семафора
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Использование: client <shm_id>\n");
        return 1;
    }

    signal(SIGINT, handle_sigint); // Установка обработчика сигнала прерывания
    atexit(cleanup); // Установка функции для очистки ресурсов при завершении программы

    shm_id = atoi(argv[1]); // Получение ID разделяемой памяти из аргументов командной строки

    hotel = (Hotel *)shmat(shm_id, NULL, 0); // Подключение к разделяемой памяти

    for (int i = 0; i < 15; i++) {
        pid_t pid = fork();
        if (pid == 0) {
            printf("Запуск процесса клиента %d...\n", i + 1);
            customer(i + 1); // Запуск функции клиента с уникальным ID
            exit(0); // Завершение процесса после выполнения функции клиента
        } else if (pid < 0) {
            printf("Ошибка при создании процесса клиента %d.\n", i + 1);
            return 1;
        }
    }

    // Главный процесс входит в состояние ожидания
    for (int i = 0; i < CUSTOMER_COUNT; i++) {
        wait(NULL);
    }

    return 0;
}