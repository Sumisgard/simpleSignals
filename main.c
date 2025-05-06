// Процесс 1 открывает существующий файл и порождает потомка 2. Процесс 1 считывает N байт из файла,
// выводит их на экран и посылает сигнал SIG1 процессу 2. Процесс 2 также считывает N байт из файла,
// выводит их на экран и посылает сигнал SIG1 процессу 1. Если одному из процессов встретился конец
// файла, то он посылает другому процессу сигнал SIG2 и они оба завершаются.

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>

#define N 16  // Размер блока для чтения

int fd;
pid_t parent_pid, child_pid;

// Обработчик сигналов для родительского процесса
void parent_handler(int sig) {
    if (sig == SIGUSR1) {
        char buffer[N];
        ssize_t bytes_read = read(fd, buffer, N);
        if (bytes_read > 0) {
            // Сообщение о начале вывода родительским процессом
            write(STDOUT_FILENO, "\nParent process is writing data...\n", 36);
            write(STDOUT_FILENO, buffer, bytes_read);
            kill(child_pid, SIGUSR1);
        } else {
            // Конец файла - завершение обоих процессов
            write(STDOUT_FILENO, "\nParent: End of file reached. Sending termination signal...\n", 61);
            kill(child_pid, SIGUSR2);
            exit(0);
        }
    } else if (sig == SIGUSR2) {
        // Получен сигнал о завершении от дочернего
        write(STDOUT_FILENO, "\nParent: Terminating as requested by child process.\n", 53);
        exit(0);
    }
}

// Обработчик сигналов для дочернего процесса
void child_handler(int sig) {
    if (sig == SIGUSR1) {
        char buffer[N];
        ssize_t bytes_read = read(fd, buffer, N);
        if (bytes_read > 0) {
            // Сообщение о начале вывода дочерним процессом
            write(STDOUT_FILENO, "\nChild process is writing data...\n", 35);
            write(STDOUT_FILENO, buffer, bytes_read);
            kill(parent_pid, SIGUSR1);
        } else {
            // Конец файла - завершение обоих процессов
            write(STDOUT_FILENO, "\nChild: End of file reached. Sending termination signal...\n", 60);
            kill(parent_pid, SIGUSR2);
            exit(0);
        }
    } else if (sig == SIGUSR2) {
        // Получен сигнал о завершении от родительского
        write(STDOUT_FILENO, "\nChild: Terminating as requested by parent process.\n", 53);
        exit(0);
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <filename>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    const char *filename = argv[1];
    fd = open(filename, O_RDONLY);
    if (fd == -1) {
        perror("Failed to open file");
        exit(EXIT_FAILURE);
    }

    parent_pid = getpid();

    // Настройка обработчиков сигналов для родителя
    struct sigaction sa_parent;
    sigemptyset(&sa_parent.sa_mask);
    sa_parent.sa_flags = 0;
    sa_parent.sa_handler = parent_handler;
    sigaction(SIGUSR1, &sa_parent, NULL);
    sigaction(SIGUSR2, &sa_parent, NULL);

    // Создание дочернего процесса
    child_pid = fork();
    if (child_pid == -1) {
        perror("Fork failed");
        exit(EXIT_FAILURE);
    }

    if (child_pid == 0) {
        // Дочерний процесс: настройка обработчиков
        struct sigaction sa_child;
        sigemptyset(&sa_child.sa_mask);
        sa_child.sa_flags = 0;
        sa_child.sa_handler = child_handler;
        sigaction(SIGUSR1, &sa_child, NULL);
        sigaction(SIGUSR2, &sa_child, NULL);

        write(STDOUT_FILENO, "\nChild process started. Waiting for signal...\n", 47);
        while (1) {
            pause();  // Ожидание сигнала
        }
    } else {
        // Родительский процесс: начать чтение
        write(STDOUT_FILENO, "\nParent process started. Beginning file reading...\n", 52);
        char buffer[N];
        ssize_t bytes_read = read(fd, buffer, N);
        if (bytes_read > 0) {
            write(STDOUT_FILENO, "\nParent process is writing initial data...\n", 44);
            write(STDOUT_FILENO, buffer, bytes_read);
            kill(child_pid, SIGUSR1);
        } else {
            write(STDOUT_FILENO, "\nParent: File is empty. Terminating.\n", 38);
            kill(child_pid, SIGUSR2);
            exit(0);
        }

        while (1) {
            pause();  // Ожидание сигнала от дочернего процесса
        }
    }

    close(fd);
    return 0;
}