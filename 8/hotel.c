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

// Функция для управления семафором
void sem_op(int op) {
    struct sembuf sops = {0, op, 0};
    semop(sem_id, &sops, 1);
}

// Функция, моделирующая работу администратора гостиницы
void admin() {
    while (1) {
        sem_op(-1); // Ожидание семафора
        // Проверка номеров и назначение ожидающим клиентам
        for (int i = 0; i < HOTEL_SIZE; i++) {
            if (hotel->rooms[i] == 0 && hotel->queue_size > 0) {
                // Назначение номера первому клиенту в очереди
                hotel->rooms[i] = hotel->queue[0];
                // Удаление первого клиента из очереди
                for (int j = 0; j < hotel->queue_size - 1; j++) {
                    hotel->queue[j] = hotel->queue[j + 1];
                }
                hotel->queue_size--;
            }
        }
        sem_op(1); // Освобождение семафора
        sleep(2); // Задержка
    }
}

int main() {
    signal(SIGINT, handle_sigint); // Установка обработчика сигнала прерывания
    atexit(cleanup); // Установка функции для очистки ресурсов при завершении программы

    // Создание разделяемой памяти для структуры гостиницы
    shm_id = shmget(IPC_PRIVATE, sizeof(Hotel), IPC_CREAT | 0666);
    hotel = (Hotel *)shmat(shm_id, NULL, 0);

    printf("shm_id: %d\n", shm_id); // Вывод shm_id

    // Создание и инициализация семафора
    sem_id = semget(IPC_PRIVATE, 1, IPC_CREAT | 0666);
    semctl(sem_id, 0, SETVAL, 1);

    admin(); // Запуск функции администратора

    while (1) pause(); // Ожидание сигналов

    return 0;
}