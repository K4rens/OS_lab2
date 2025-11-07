#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <complex.h>


typedef struct {
    int thread_id;           
    int num_threads;         
    int max_threads;         
    int rows_a, cols_a;      
    int rows_b, cols_b;      
    double complex **matrix_a; // Указатель на матрицу A
    double complex **matrix_b; // Указатель на матрицу B
    double complex **result;   // Указатель на результирующую матрицу
    int start_row;           
    int end_row;             
    HANDLE start_semaphore;  // Семафор для синхронизации старта потоков
    HANDLE finish_mutex;     // Мьютекс для синхронизации доступа к общим данным
    int *active_threads;     // Указатель на счетчик активных потоков
} ThreadData;

double complex** create_matrix(int rows, int cols);
void fill_matrix_random(double complex **matrix, int rows, int cols);
void free_matrix(double complex **matrix, int rows);
void print_matrix(double complex **matrix, int rows, int cols);
DWORD WINAPI matrix_multiply_thread(LPVOID param);


int main(int argc, char *argv[]) {
    printf("=== Multiplying matrixes with compl nums ===\n");
    
    int max_threads = 4; // значение по умолчанию
    if (argc > 1) {
        max_threads = atoi(argv[1]);
        if (max_threads <= 0) {
    printf("=== Error: Do positive number ===\n");
            return 1;
        }
    }
    printf("Maximum threads: %d\n", max_threads);

    int rows_a = 4, cols_a = 4;
    int rows_b = 4, cols_b = 4;

    // Можно ли перемножать матрицы?
    if (cols_a != rows_b) {
        printf("Error: Matrix dimensions incompatible for multiplication\n");
        return 1;
    }

    // Создание матриц
    printf("\nCreating matrix...\n");
    double complex **matrix_a = create_matrix(rows_a, cols_a);
    double complex **matrix_b = create_matrix(rows_b, cols_b);
    double complex **result = create_matrix(rows_a, cols_b);

    // Заполнение матриц случайными комплексными числами
    srand((unsigned int)time(NULL));
    fill_matrix_random(matrix_a, rows_a, cols_a);
    fill_matrix_random(matrix_b, rows_b, cols_b);

    printf("\nMatrix A:\n");
    print_matrix(matrix_a, rows_a, cols_a);
    
    printf("\nMatrix B:\n");
    print_matrix(matrix_b, rows_b, cols_b);

    // Сколько потоков создать
    int actual_threads = (max_threads < rows_a) ? max_threads : rows_a;
    printf("\nCreating %d worker threads\n", actual_threads);

    // Управление потоками
    HANDLE *threads = (HANDLE*)malloc(actual_threads * sizeof(HANDLE));
    ThreadData *thread_data = (ThreadData*)malloc(actual_threads * sizeof(ThreadData));
    
    // Объекты синхронизации
    HANDLE start_semaphore = CreateSemaphore(NULL, 0, actual_threads, NULL);  //дефолт безопасность, нач знач, макс знач, имя
    HANDLE finish_mutex = CreateMutex(NULL, FALSE, NULL); //безопасность, не захват мьютекса, ноу имени
    int active_threads = 0;

    // Вычисление количества строк на поток
    int rows_per_thread = rows_a / actual_threads;
    int extra_rows = rows_a % actual_threads;

    // Создание потоков
    int current_row = 0;
    for (int i = 0; i < actual_threads; i++) {
        // Заполнение структуры данных для потока
        thread_data[i].thread_id = i;
        thread_data[i].num_threads = actual_threads;
        thread_data[i].max_threads = max_threads;
        thread_data[i].rows_a = rows_a;
        thread_data[i].cols_a = cols_a;
        thread_data[i].rows_b = rows_b;
        thread_data[i].cols_b = cols_b;
        thread_data[i].matrix_a = matrix_a;
        thread_data[i].matrix_b = matrix_b;
        thread_data[i].result = result;
        thread_data[i].start_semaphore = start_semaphore;
        thread_data[i].finish_mutex = finish_mutex;
        thread_data[i].active_threads = &active_threads;

        // Распределение строк между потоками
        thread_data[i].start_row = current_row;   // Для i устанавливаем строку текущую, чтобы отслеживать где мы 
        int rows_this_thread = rows_per_thread + (i < extra_rows ? 1 : 0);  // Вычисление колва строк для этого потока 
        thread_data[i].end_row = current_row + rows_this_thread; // Уст конечную строку для этого потока
        current_row = thread_data[i].end_row;

        // Создание потока
        threads[i] = CreateThread(NULL, 0, matrix_multiply_thread, &thread_data[i], 0, NULL);
        // деф безопасность, разм стека деф, функция, которую будет выполнять поток, параметры созданные сверху, запуск сразу, без id
        if (threads[i] == NULL) {
            printf("Error creating thread %d\n", i);
            return 1;
        }
    }

    // Измерение производительности
    clock_t start_time = clock();
    
    // Одновременный запуск всех потоков
    ReleaseSemaphore(start_semaphore, actual_threads, NULL); 

    // Ожидание завершения всех потоков
    WaitForMultipleObjects(actual_threads, threads, TRUE, INFINITE); 

    clock_t end_time = clock();
    double execution_time = ((double)(end_time - start_time)) / CLOCKS_PER_SEC;

    // Запись результатов
    printf("\nResult matrix (A * B):\n");
    print_matrix(result, rows_a, cols_b);

    printf("\nPerformance results:\n");
    printf("Execution time: %.6f seconds\n", execution_time);
    printf("Number of threads used: %d\n", actual_threads);

    

    // Очистка ресурсов
    for (int i = 0; i < actual_threads; i++) {
        CloseHandle(threads[i]);
    }
    CloseHandle(start_semaphore);
    CloseHandle(finish_mutex);
    
    free(threads);
    free(thread_data);
    free_matrix(matrix_a, rows_a);
    free_matrix(matrix_b, rows_b);
    free_matrix(result, rows_a);


    printf("\nProgram completed successfully!\n");
    return 0;
}

// Создание матрицы
double complex** create_matrix(int rows, int cols) {
    double complex **matrix = (double complex**)malloc(rows * sizeof(double complex*));
    for (int i = 0; i < rows; i++) {
        matrix[i] = (double complex*)malloc(cols * sizeof(double complex));
    }
    return matrix;
}

// Заполнение матрицы случайными комплексными числами
void fill_matrix_random(double complex **matrix, int rows, int cols) {
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            double real = (double)(rand() % 100) / 10.0;  // Действительная часть
            double imag = (double)(rand() % 100) / 10.0;  // Мнимая часть
            matrix[i][j] = real + imag * I;
        }
    }
}

// Освобождение памяти матрицы
void free_matrix(double complex **matrix, int rows) {
    for (int i = 0; i < rows; i++) {
        free(matrix[i]);
    }
    free(matrix);
}

// Вывод матрицы
void print_matrix(double complex **matrix, int rows, int cols) {
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            double real = creal(matrix[i][j]);
            double imag = cimag(matrix[i][j]);
            if (imag >= 0) {
                printf("%.1f + %.1fi\t", real, imag);
            } else {
                printf("%.1f - %.1fi\t", real, -imag);
            }
        }
        printf("\n");
    }
}

// Функция потока для умножения матриц
DWORD WINAPI matrix_multiply_thread(LPVOID param) {
    ThreadData *data = (ThreadData*)param;
    
    // Ожидание сигнала начала работы
    WaitForSingleObject(data->start_semaphore, INFINITE);

    // Обновление счетчика активных потоков
    WaitForSingleObject(data->finish_mutex, INFINITE);
    (*(data->active_threads))++;
    printf("Thread %d started. Active threads: %d\n", 
           data->thread_id, *(data->active_threads));
    ReleaseMutex(data->finish_mutex);

    // Умнжение
    for (int i = data->start_row; i < data->end_row; i++) {
        for (int j = 0; j < data->cols_b; j++) {
            data->result[i][j] = 0 + 0*I; 
            
            for (int k = 0; k < data->cols_a; k++) {
                // Комплексное умножение
                data->result[i][j] += data->matrix_a[i][k] * data->matrix_b[k][j];
            }
        }
    }

    // Обновление счетчика активных потоков при завершении
    WaitForSingleObject(data->finish_mutex, INFINITE);
    (*(data->active_threads))--;
    printf("Thread %d finished. Active threads: %d\n", 
           data->thread_id, *(data->active_threads));
    ReleaseMutex(data->finish_mutex);

    return 0;
}

