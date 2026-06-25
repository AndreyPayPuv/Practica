#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <commdlg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <time.h>
#include <locale.h>



/*  "ЯДРО" АЛГОРИТМА: сортировка Шелла, чтение/запись CSV, генерация тестовых данных,сравнение с qsort.*/

#define MAX_SIZE 1000000       // Максимальный размер массива для сортировки
#define MAX_LINE_LEN 8000000   // Максимальная длина строки при чтении CSV файла

typedef struct {
	long comparisons;
	long swaps;
	double time_seconds;
} SortStats;

// КОДЫ ОШИБОК ПРИ РАБОТЕ С CSV ФАЙЛАМИ
enum {
	CSV_OK = 0,
	CSV_ERR_OPEN = -1,
	CSV_ERR_EMPTY = -2,
	CSV_ERR_FORMAT = -3,
	CSV_ERR_TOO_LARGE = -4
};

// ТИПЫ ГЕНЕРИРУЕМЫХ ДАННЫХ
enum {
	DATA_SORTED = 1,
	DATA_REVERSED = 2,
	DATA_RANDOM = 3,
	DATA_REPEATED = 4
};

// Возвращает текстовое описание ошибки по её коду
static const char* csv_error_message(int code)
{
	switch (code) {
	case CSV_OK:            return "OK";
	case CSV_ERR_OPEN:      return "не удалось открыть файл";
	case CSV_ERR_EMPTY:     return "файл пуст или не содержит данных";
	case CSV_ERR_FORMAT:    return "входные данные некорректны (не числа или пустые поля)";
	case CSV_ERR_TOO_LARGE: return "во входном файле слишком много элементов";
	default:                return "неизвестная ошибка";
	}
}

static void trim_newline(char* s)
{
	size_t len = strlen(s);
	while (len > 0 && (s[len - 1] == '\n' || s[len - 1] == '\r')) {
		s[len - 1] = '\0';
		len--;
	}
}



// АЛГОРИТМ СОРТИРОВКИ ШЕЛЛА
static void shell_sort(long* array, int len, SortStats* stats)
{
	long h;
	int i, j;
	long temp;
	clock_t start, end;

	stats->comparisons = 0;
	stats->swaps = 0;

	start = clock();

	h = 1;
	while (h < len / 3) {
		h = 3 * h + 1;
	}

	while (h >= 1) {
		for (i = h; i < len; i++) {
			temp = array[i];
			j = i;

			while (j >= h) {
				stats->comparisons++;
				if (array[j - h] > temp) {
					array[j] = array[j - h];
					stats->swaps++;
					j -= h;
				}
				else {
					break;
				}
			}

			if (j != i) {
				array[j] = temp;
			}
		}

		h /= 3;
	}

	end = clock();
	stats->time_seconds = (double)(end - start) / CLOCKS_PER_SEC;
}

// ФУНКЦИЯ СРАВНЕНИЯ ДЛЯ QSORT
static int compare_long(const void* a, const void* b)
{
	long va = *(const long*)a;
	long vb = *(const long*)b;

	if (va < vb) return -1;
	if (va > vb) return 1;
	return 0;
}

// Запускает стандартную qsort и замеряет её время работы
static void measure_qsort(long* array, int len, SortStats* stats)
{
	clock_t start, end;

	start = clock();
	qsort(array, (size_t)len, sizeof(long), compare_long);
	end = clock();

	stats->time_seconds = (double)(end - start) / CLOCKS_PER_SEC;
	stats->comparisons = -1;
	stats->swaps = -1;
}