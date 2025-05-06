// Процесс 1 открывает существующий файл и порождает потомка 2. Процесс 1 считывает N байт из файла,
// выводит их на экран и посылает сигнал SIG1 процессу 2. Процесс 2 также считывает N байт из файла,
// выводит их на экран и посылает сигнал SIG1 процессу 1. Если одному из процессов встретился конец
// файла, то он посылает другому процессу сигнал SIG2 и они оба завершаются.

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <sys/types.h>

volatile sig_atomic_t sig1_flag = 0;
volatile sig_atomic_t sig2_flag = 0;

pid_t other_pid;

void sig1_handler(int sig) {
    sig1_flag = 1;
}

void sig2_handler(int sig) {
    sig2_flag = 1;
}

void error(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <filename> <N>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    const char *filename = argv[1];
    int N = atoi(argv[2]);
    if (N <= 0) {
        fprintf(stderr, "N must be positive\n");
        exit(EXIT_FAILURE);
    }

    int fd = open(filename, O_RDONLY);
    if (fd == -1) {
        perror("open");
        exit(EXIT_FAILURE);
    }

    pid_t child_pid = fork();
    if (child_pid == -1) {
        perror("fork");
        close(fd);
        exit(EXIT_FAILURE);
    }

    // Установка обработчиков сигналов
    struct sigaction sa1, sa2;
    memset(&sa1, 0, sizeof(sa1));
    sa1.sa_handler = sig1_handler;
    sa1.sa_flags = SA_RESTART;
    if (sigaction(SIGUSR1, &sa1, NULL) == -1) {
        perror("sigaction SIGUSR1");
        exit(EXIT_FAILURE);
    }

    memset(&sa2, 0, sizeof(sa2));
    sa2.sa_handler = sig2_handler;
    sa2.sa_flags = SA_RESTART;
    if (sigaction(SIGUSR2, &sa2, NULL) == -1) {
        perror("sigaction SIGUSR2");
        exit(EXIT_FAILURE);
    }

    // Блокируем сигналы SIGUSR1 и SIGUSR2
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGUSR1);
    sigaddset(&mask, SIGUSR2);
    if (sigprocmask(SIG_BLOCK, &mask, NULL) == -1) {
        perror("sigprocmask");
        exit(EXIT_FAILURE);
    }

    if (child_pid == 0) { // процесс 2
        other_pid = getppid(); // PID родителя

        while (1) {
            // Ожидаем сигнала SIGUSR1 от родителя
            sig1_flag = 0;
            sig2_flag = 0;

            // Создаем маску для ожидания
            sigset_t wait_mask;
            if (sigprocmask(0, NULL, &wait_mask) == -1) {
                perror("sigprocmask");
                exit(EXIT_FAILURE);
            }
            sigdelset(&wait_mask, SIGUSR1);
            sigdelset(&wait_mask, SIGUSR2);

            while (!sig1_flag && !sig2_flag) {
                sigsuspend(&wait_mask); // ожидаем сигнал
            }

            if (sig2_flag) {
                exit(EXIT_SUCCESS);
            }

            // Читаем N байт
            char *buffer = malloc(N + 1);
            if (!buffer) {
                perror("malloc");
                kill(other_pid, SIGUSR2);
                exit(EXIT_FAILURE);
            }

            int bytes_read = read(fd, buffer, N);
            if (bytes_read == -1) {
                perror("read");
                kill(other_pid, SIGUSR2);
                free(buffer);
                exit(EXIT_FAILURE);
            }

            if (bytes_read == 0) { // EOF
                kill(other_pid, SIGUSR2);
                free(buffer);
                exit(EXIT_SUCCESS);
            }

            buffer[bytes_read] = '\0';
            printf("Process 2: %.*s\n", bytes_read, buffer);
            free(buffer);

            // Отправляем SIGUSR1 родителю
            if (kill(other_pid, SIGUSR1) == -1) {
                exit(EXIT_FAILURE);
            }
        }
    } else { // процесс 1
        other_pid = child_pid;

        while (1) {
            // Читаем N байт
            char *buffer = malloc(N + 1);
            if (!buffer) {
                perror("malloc");
                kill(other_pid, SIGUSR2);
                close(fd);
                exit(EXIT_FAILURE);
            }

            int bytes_read = read(fd, buffer, N);
            if (bytes_read == -1) {
                perror("read");
                kill(other_pid, SIGUSR2);
                free(buffer);
                close(fd);
                exit(EXIT_FAILURE);
            }

            if (bytes_read == 0) { // EOF
                kill(other_pid, SIGUSR2);
                free(buffer);
                close(fd);
                exit(EXIT_SUCCESS);
            }

            buffer[bytes_read] = '\0';
            printf("Process 1: %.*s\n", bytes_read, buffer);
            free(buffer);

            // Отправляем SIGUSR1 дочернему
            if (kill(other_pid, SIGUSR1) == -1) {
                exit(EXIT_FAILURE);
            }

            // Ожидаем сигнала от дочернего
            sig1_flag = 0;
            sig2_flag = 0;

            // Создаем маску для ожидания
            sigset_t wait_mask;
            if (sigprocmask(0, NULL, &wait_mask) == -1) {
                perror("sigprocmask");
                exit(EXIT_FAILURE);
            }
            sigdelset(&wait_mask, SIGUSR1);
            sigdelset(&wait_mask, SIGUSR2);

            while (!sig1_flag && !sig2_flag) {
                sigsuspend(&wait_mask); // ожидаем
            }

            if (sig2_flag) {
                close(fd);
                exit(EXIT_SUCCESS);
            }
        }
    }

    close(fd);
    return 0;
}