#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <semaphore.h>

struct Entry
{
    char name[11];
    double price;
};

const char *names[11] = {"ALUMINIUM", "COPPER", "COTTON", "CRUDEOIL", "GOLD", "LEAD",
                         "MENTHAOIL", "NATURALGAS", "NICKEL", "SILVER", "ZINC"};
const char *colors[3] = {"\033[36m", "\033[32m", "\033[31m"};
const char *arrows[3] = {" ", "↑", "↓"};

double price[11];
double average[11];
double history[11][5];
int count[11];
int priceArrow[11];
int averageArrow[11];

int getArrow(double now, double before)
{
    if (now > before)
        return 1;
    if (now < before)
        return 2;
    return 0;
}

void draw()
{
    printf("\033[1;1H\033[2J");
    printf("+-----------------------------------+\n");
    printf("| %-11s | %-8s | %-8s |\n", "Currency", "Price", "AvgPrice");
    printf("+-----------------------------------+\n");

    for (int i = 0; i < 11; i++)
    {
        printf("| %-11s | ", names[i]);
        printf("%s%7.2lf%s\033[0m", colors[priceArrow[i]], price[i], arrows[priceArrow[i]]);
        printf(" | ");
        printf("%s%7.2lf%s\033[0m", colors[averageArrow[i]], average[i], arrows[averageArrow[i]]);
        printf(" |\n");
    }

    printf("+-----------------------------------+\n");
    fflush(stdout);
}

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        printf("Usage: ./consumer BUFFER_SIZE\n");
        return 1;
    }

    int n = atoi(argv[1]);

    int size = 2 * sizeof(int) + n * sizeof(Entry);
    int fd = shm_open("/pc_shm", O_CREAT | O_RDWR, 0666);
    ftruncate(fd, size);
    char *memory = (char *)mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

    int *in = (int *)memory;
    int *out = in + 1;
    Entry *entries = (Entry *)(in + 2);

    sem_t *mutex = sem_open("/pc_mutex", O_CREAT, 0666, 1);
    sem_t *empty = sem_open("/pc_empty", O_CREAT, 0666, n);
    sem_t *full = sem_open("/pc_full", O_CREAT, 0666, 0);

    draw();

    while (true)
    {
        sem_wait(full);
        sem_wait(mutex);

        Entry entry = entries[*out];
        *out = (*out + 1) % n;

        sem_post(mutex);
        sem_post(empty);

        int k = -1;
        for (int i = 0; i < 11; i++)
        {
            if (strcmp(entry.name, names[i]) == 0)
                k = i;
        }
        if (k == -1)
            continue;

        history[k][count[k] % 5] = entry.price;
        count[k]++;

        int total = count[k];
        if (total > 5)
            total = 5;
        double sum = 0;
        for (int i = 0; i < total; i++)
            sum = sum + history[k][i];
        double newAverage = sum / total;

        priceArrow[k] = getArrow(entry.price, price[k]);
        averageArrow[k] = getArrow(newAverage, average[k]);
        price[k] = entry.price;
        average[k] = newAverage;

        draw();
    }

    return 0;
}
