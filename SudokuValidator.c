
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/syscall.h>

int SUDOKU_SIZE = 9;
int sudoku[9][9];
int isValid = 1;

// Helper function
void print_array(int v[9][9])
{
    int i, j;
    for (i = 0; i < 9; i++)
    {
        for (j = 0; j < 9; j++)
        {
            printf("%d ", v[i][j]);
        }
        printf("\n");
    }
}

#define handle_error(msg)   \
    do                      \
    {                       \
        perror(msg);        \
        exit(EXIT_FAILURE); \
    } while (0)

void validate_rows()
{
    int col, row;
    omp_set_nested(1);
    omp_set_num_threads(9);
#pragma omp parallel for private(col, row) schedule(dynamic)
    for (row = 0; row < SUDOKU_SIZE; row++)
    {
        int values[9] = {0, 0, 0, 0, 0, 0, 0, 0, 0};
        for (col = 0; col < SUDOKU_SIZE; col++)
        {
            int value = sudoku[row][col];
            if (value <= 9 && value > 0)
            {
                values[value - 1]++;
            }
            else
            {
                isValid = 0;
            }
        }
        int i;
        for (i = 0; i < SUDOKU_SIZE; i++)
        {
            isValid = (int)(isValid & (int)(values[i] == 1));
        }
    }
}

void validate_columns()
{
    omp_set_nested(1);
    omp_set_num_threads(9);
    int col, row;
#pragma omp parallel for private(col, row) schedule(dynamic)
    for (col = 0; col < SUDOKU_SIZE; col++)
    {
        int values[9] = {0, 0, 0, 0, 0, 0, 0, 0, 0};
        for (row = 0; row < SUDOKU_SIZE; row++)
        {
            int value = sudoku[row][col];
            if (value <= 9 && value > 0)
            {
                values[value - 1]++;
            }
            else
            {
                isValid = 0;
            }
        }
        int i;
        for (i = 0; i < SUDOKU_SIZE; i++)
        {
            isValid = (int)(isValid & (int)(values[i] == 1));
        }
    }
}

void validate_section()
{
    int col, row, margin;
    omp_set_nested(1);
    omp_set_num_threads(9);
#pragma omp parallel for private(margin) schedule(dynamic)
    for (margin = 0; margin < 3; margin++)
    {
        int values[9] = {0, 0, 0, 0, 0, 0, 0, 0, 0};
        omp_set_num_threads(9);
#pragma omp parallel for private(col, row) schedule(dynamic)
        for (col = margin * 3; col < ((1 + margin) * 3); col++)
        {
            // for (row_margin = 0; row_margin < 3; row_margin++)
            // {

            for (row = margin * 3; row < ((1 + margin) * 3); row++)
            {
                int value = sudoku[row][col];
                if (value <= 9 && value > 0)
                {
                    values[value - 1]++;
                }
                else
                {
                    isValid = 0;
                }
            }
            // }
        }
        int i;
        for (i = 0; i < SUDOKU_SIZE; i++)
        {
            isValid = (int)(isValid & (int)(values[i] == 1));
        }
    }
}

void *check_columns()
{
    // We get the thread id
    printf("Thread id: %d\n", (int)syscall(SYS_gettid));
    //We check the columns
    validate_columns();
    //We exit the thread
    pthread_exit(0);
}

int main(int argc, char const *argv[])
{
    omp_set_num_threads(1);
    omp_set_nested(1);
    char *mapped_area;
    int file_descriptor;
    struct stat stat_file;
    file_descriptor = open((char *)argv[1], O_RDWR);

    if (file_descriptor < 0)
        handle_error("open");

    if (fstat(file_descriptor, &stat_file) == -1)
        handle_error("fstat");

    //It can read, and write those are the permissions
    mapped_area = (char *)mmap(0, stat_file.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, file_descriptor, 0);
    if (mapped_area < 0)
        handle_error("mmap");

    close(file_descriptor);
    int row;
    row = 0;
    int j;
    /* fill 9x9 array with file values */
    for (j = 0; j < stat_file.st_size; j++)
    {
        if ((j > 0) & (int)(j % 9 == 0))
        {
            row++;
        }
        int representation = (char)mapped_area[j] - '0';
        sudoku[row][j % 9] = representation;
    }

    //    print_array(sudoku);
    validate_section();
    pid_t pid, forkId;
    pid = getpid();

    printf("Parent id: %d\n", (int)pid);
    forkId = fork();
    if (forkId == -1)
        handle_error("fork");

    //We check if it is the child
    if (forkId == 0)
    {
        int ps;
        //The representation of the id in string so we can store it
        char id_rep[5];
        snprintf(id_rep, sizeof(id_rep), "%d", (int)getppid());
        ps = execlp("/bin/ps", "ps", "-lLf", "p", id_rep, NULL);
    }
    else
    {
        //Here we are in the parent process
        pthread_t tid;
        pthread_attr_t attr;
        pid_t forkId2;
        pthread_attr_init(&attr);
        pthread_create(&tid, &attr, check_columns, NULL);
        pthread_join(tid, NULL);

        //We will wait for ps
        wait(NULL);

        validate_rows();

        if (isValid == 1)
        {
            printf("The sudoku is valid\n");
        }
        else
        {
            printf("Wrong sudoku\n");
        }

        forkId2 = fork();

        if (forkId2 == 0)
        {
            //Its the child from the fork so we execute ps 2 again
            int ps2;
            char id_rep[5];
            snprintf(id_rep, sizeof(id_rep), "%d", (int)getppid());
            ps2 = execlp("/bin/ps", "ps", "-lLf", "p", id_rep, NULL);
        }
        else
        {
            wait(NULL);
            return 0;
        }
    }

    return 0;
}
