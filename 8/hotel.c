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

// Функция для очистки ресурсов
void cleanup() {
    printf("Очистка...\n");
    semctl(semid, 0, IPC_RMID); // Удаление семафора
    shmdt(hotel); // Отсоединение разделяемой памяти
    shmctl(shmid, IPC_RMID, NULL); // Удаление разделяемой памяти
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
        printf("Администратор проверяет номера...\n");
        // Проверка номеров и назначение ожидающим клиентам
        int occupied_rooms = 0;
        for (int i = 0; i < HOTEL_SIZE; i++) {
            if (hotel->rooms[i] == 0 && hotel->queue_size > 0) {
                struct sembuf sb = {0, -1, 0}; // Операция ожидания семафора
                semop(semid, &sb, 1);
                // Назначение номера первому клиенту в очереди
                hotel->rooms[i] = hotel->queue[0];
                printf("Клиент %d заселился в номер %d.\n", hotel->queue[0], i);
                // Удаление первого клиента из очереди
                for (int j = 0; j < hotel->queue_size - 1; j++) {
                    hotel->queue[j] = hotel->queue[j + 1];
                }
                hotel->queue_size--;
                occupied_rooms++;
                sb.sem_op = 1; // Операция освобождения семафора
                semop(semid, &sb, 1);
            } else if (hotel->rooms[i] != 0) {
                printf("Номер %d занят клиентом %d.\n", i, hotel->rooms[i]);
                occupied_rooms++;
            } else {
                printf("Номер %d свободен.\n", i);
            }
        }
        printf("Занято %d из %d номеров.\n", occupied_rooms, HOTEL_SIZE);
        if (occupied_rooms == HOTEL_SIZE && hotel->queue_size == 0) {
            printf("Все номера заняты и нет клиентов в очереди. Администратор ждет...\n");
            sleep(2); // Задержка
        }
    }
}

int main() {
    signal(SIGINT, handle_sigint); // Установка обработчика сигнала прерывания
    atexit(cleanup); // Установка функции для очистки ресурсов при завершении программы

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

    printf("Запуск процесса администратора...\n");
    admin();

    return 0;
}