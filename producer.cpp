#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <signal.h>
#include <sys/mman.h>
#include <semaphore.h>
#include <random>

struct Entry
{
    char name[11];
    double price;
};

char *memory;
int size;
sem_t *mutex;
sem_t *empty;
sem_t *full;
sem_t *users;

void cleanup(int sig)
{
    sem_trywait(users);
    int count;
    sem_getvalue(users, &count);

    munmap(memory, size);
    sem_close(mutex);
    sem_close(empty);
    sem_close(full);
    sem_close(users);

    if (count == 0)
    {
        sem_unlink("/mutex");
        sem_unlink("/empty");
        sem_unlink("/full");
        sem_unlink("/users");
        shm_unlink("/shm");
    }

    _exit(0);
}

void logMessage(const char *name, const char *text)
{
    struct timespec now;
    clock_gettime(CLOCK_REALTIME, &now);

    struct tm *t = localtime(&now.tv_sec);
    char timeText[30];
    strftime(timeText, sizeof(timeText), "%m/%d/%Y %H:%M:%S", t);

    fprintf(stderr, "[%s.%03ld] %s: %s\n", timeText, now.tv_nsec / 1000000, name, text);
}

int main(int argc, char *argv[])
{
    if (argc != 6)
    {
        printf("Usage: ./producer NAME MEAN STDDEV SLEEP_MS BUFFER_SIZE\n");
        return 1;
    }

    char *name = argv[1];
    double mean = atof(argv[2]);
    double stddev = atof(argv[3]);
    int sleepTime = atoi(argv[4]);
    int n = atoi(argv[5]);

    size = 2 * sizeof(int) + n * sizeof(Entry);
    int fd = shm_open("/shm", O_CREAT | O_RDWR, 0666);
    ftruncate(fd, size);
    memory = (char *)mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);

    int *in = (int *)memory;
    Entry *entries = (Entry *)(in + 2);

    mutex = sem_open("/mutex", O_CREAT, 0666, 1);
    empty = sem_open("/empty", O_CREAT, 0666, n);
    full = sem_open("/full", O_CREAT, 0666, 0);
    users = sem_open("/users", O_CREAT, 0666, 0);

    sem_post(users);
    signal(SIGINT, cleanup);
    signal(SIGTERM, cleanup);

    std::random_device device;
    std::mt19937 generator(device());
    std::normal_distribution<double> distribution(mean, stddev);

    char text[100];

    while (true)
    {
        double price = distribution(generator);
        sprintf(text, "generating a new value %.2lf", price);
        logMessage(name, text);

        sem_wait(empty);

        logMessage(name, "trying to get mutex on shared buffer");
        sem_wait(mutex);

        sprintf(text, "placing %.2lf on shared buffer", price);
        logMessage(name, text);
        strcpy(entries[*in].name, name);
        entries[*in].price = price;
        *in = (*in + 1) % n;

        sem_post(mutex);
        sem_post(full);

        sprintf(text, "sleeping for %d ms", sleepTime);
        logMessage(name, text);
        usleep(sleepTime * 1000);
    }

    return 0;
}
