#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
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
int semid; // ID семафора
int shmid; // ID разделяемой памяти

// Функция, моделирующая поведение клиента
void customer(int id) {
    srand(time(NULL) + id); // Инициализация генератора случайных чисел
    int stay_duration = (rand() % 2 + 1) * 2; // Случайный выбор продолжительности пребывания (2 или 4 секунды)
    printf("Клиент %d будет жить в номере %d секунды.\n", id, stay_duration);

    while (1) {
        // Проверка, находится ли клиент в очереди
        int in_queue = 0;
        for (int i = 0; i < hotel->queue_size; i++) {
            if (hotel->queue[i] == id) {
                in_queue = 1;
                printf("Клиент %d находится в очереди на позиции %d.\n", id, i+1);
                break;
            }
        }
        // Если клиент не в очереди, запрос номера
        if (!in_queue) {
            struct sembuf sb = {0, -1, 0}; // Операция ожидания семафора
            semop(semid, &sb, 1);
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
            sb.sem_op = 1; // Операция освобождения семафора
            semop(semid, &sb, 1);
        }
        sleep(stay_duration); // Сон на время пребывания

        // После пребывания, освобождение номера
        struct sembuf sb = {0, -1, 0}; // Операция ожидания семафора
        semop(semid, &sb, 1);
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
        sb.sem_op = 1; // Операция освобождения семафора
        semop(semid, &sb, 1);
    }
}

int main() {
    // Получение доступа к разделяемой памяти для структуры гостиницы
    key_t key = ftok("HOTEL", 'H');
    shmid = shmget(key, sizeof(Hotel), IPC_CREAT | 0666);
    hotel = shmat(shmid, NULL, 0);

    // Получение доступа к семафору
    semid = semget(key, 1, IPC_CREAT | 0666);

    // Check if the shared memory and semaphore are correctly initialized and attached
    if (shmid == -1 || semid == -1) {
        perror("Failed to get shared memory or semaphore");
        exit(1);
    }

    // Check if the shared memory is correctly attached
    if (hotel == (void *) -1) {
        perror("Failed to attach shared memory");
        exit(1);
    }

    // Создание процессов для каждого клиента
    for (int id = 1; id <= CUSTOMER_COUNT; id++) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("Failed to fork");
            exit(1);
        } else if (pid == 0) {
            printf("Запуск процесса клиента %d...\n", id);
            customer(id);
            exit(0);
        }
    }

    // Ожидание завершения всех процессов клиентов
    for (int id = 1; id <= CUSTOMER_COUNT; id++) {
        wait(NULL);
    }

    return 0;
}