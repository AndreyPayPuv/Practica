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

#pragma comment(linker, "\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

#pragma execution_character_set("utf-8")

// Максимальный размер массива для сортировки
#define MAX_SIZE 1000000
// Максимальная длина строки при чтении CSV файла
#define MAX_LINE_LEN 8000000


// Структура содержит метрики производительности алгоритма сортировки
typedef struct {
    long comparisons;      // Количество сравнений элементов
    long swaps;            // Количество перестановок/сдвигов элементов
    double time_seconds;   // Время выполнения в секундах
} SortStats;

// КОДЫ ОШИБОК ПРИ РАБОТЕ С CSV ФАЙЛАМИ
enum {
    CSV_OK = 0,              // Успешное выполнение
    CSV_ERR_OPEN = -1,       // Не удалось открыть файл
    CSV_ERR_EMPTY = -2,      // Файл пуст или не содержит данных
    CSV_ERR_FORMAT = -3,     // Некорректный формат данных
    CSV_ERR_TOO_LARGE = -4   // Превышен максимальный размер данных
};

// ТИПЫ ГЕНЕРИРУЕМЫХ ДАННЫХ
enum {
    DATA_SORTED = 1,     // Отсортированный массив
    DATA_REVERSED = 2,   // Инвертированный массив
    DATA_RANDOM = 3,     // Случайный массив
    DATA_REPEATED = 4    // Массив с повторяющимися элементами
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

// Удаляет символы \n и \r в конце строки
static void trim_newline(char* s)
{
    size_t len = strlen(s);
    while (len > 0 && (s[len - 1] == '\n' || s[len - 1] == '\r')) {
        s[len - 1] = '\0';
        len--;
    }
}

// ФУНКЦИЯ ЧТЕНИЯ МАССИВА ИЗ CSV ФАЙЛА
// Параметры:
//   filename - путь к файлу (в широких символах)
//   array - массив для хранения прочитанных данных
//   max_len - максимальный размер массива
//   out_len - выходной параметр, количество прочитанных элементов
static int read_csv_array(const wchar_t* filename, long* array, int max_len, int* out_len)
{
    FILE* fp;
    char* line = NULL;
    size_t capacity = 4096;
    size_t length = 0;
    int found_nonempty = 0;
    int count = 0;
    char* cursor;

    // Открытие файла в режиме чтения
    fp = _wfopen(filename, L"r");
    if (fp == NULL) {
        return CSV_ERR_OPEN;
    }

    // Выделение памяти для буфера строки
    line = (char*)malloc(capacity);
    if (line == NULL) {
        fclose(fp);
        return CSV_ERR_OPEN;
    }
    line[0] = '\0';

    // Чтение файла по частям для обработки длинных строк
    while (1) {
        char chunk[4096];
        size_t chunk_len;

        if (fgets(chunk, sizeof(chunk), fp) == NULL) {
            break;
        }

        chunk_len = strlen(chunk);

        // Проверка необходимости расширения буфера
        if (length + chunk_len + 1 > capacity) {
            size_t new_capacity = capacity * 2;
            char* new_line;

            // Увеличение буфера до необходимого размера
            while (new_capacity < length + chunk_len + 1) {
                new_capacity *= 2;
            }
            if (new_capacity > MAX_LINE_LEN) {
                free(line);
                fclose(fp);
                return CSV_ERR_TOO_LARGE;
            }
            new_line = (char*)realloc(line, new_capacity);
            if (new_line == NULL) {
                free(line);
                fclose(fp);
                return CSV_ERR_TOO_LARGE;
            }
            line = new_line;
            capacity = new_capacity;
        }

        // Копирование прочитанной части в основной буфер
        memcpy(line + length, chunk, chunk_len + 1);
        length += chunk_len;

        // Проверка достижения конца строки
        if (chunk_len > 0 && chunk[chunk_len - 1] == '\n') {
            trim_newline(line);
            length = strlen(line);
            if (length > 0) {
                found_nonempty = 1;
                break;
            }
            length = 0;
            line[0] = '\0';
            continue;
        }
    }

    fclose(fp);

    // Финальная обработка прочитанной строки
    trim_newline(line);
    if (strlen(line) > 0) {
        found_nonempty = 1;
    }

    if (!found_nonempty) {
        free(line);
        return CSV_ERR_EMPTY;
    }

    // Парсинг CSV строки - разделение по запятым
    cursor = line;
    while (1) {
        char* comma = strchr(cursor, ',');
        char field[64];
        size_t field_len;
        char* start, * end, * endptr;
        long value;

        // Определение длины текущего поля
        field_len = (comma != NULL) ? (size_t)(comma - cursor) : strlen(cursor);

        if (field_len == 0 || field_len >= sizeof(field)) {
            free(line);
            return CSV_ERR_FORMAT;
        }

        // Копирование поля во временный буфер
        memcpy(field, cursor, field_len);
        field[field_len] = '\0';

        // Удаление пробелов в начале и конце поля
        start = field;
        while (isspace((unsigned char)*start)) {
            start++;
        }
        end = start + strlen(start);
        while (end > start && isspace((unsigned char)*(end - 1))) {
            *(--end) = '\0';
        }

        if (strlen(start) == 0) {
            free(line);
            return CSV_ERR_FORMAT;
        }

        // Преобразование строки в число
        endptr = NULL;
        value = strtol(start, &endptr, 10);
        if (endptr == start || *endptr != '\0') {
            free(line);
            return CSV_ERR_FORMAT;
        }

        // Проверка переполнения массива
        if (count >= max_len) {
            free(line);
            return CSV_ERR_TOO_LARGE;
        }

        // Сохранение значения в массив
        array[count] = value;
        count++;

        if (comma == NULL) {
            break;
        }
        cursor = comma + 1;
    }

    free(line);

    if (count == 0) {
        return CSV_ERR_EMPTY;
    }

    *out_len = count;
    return CSV_OK;
}

// ФУНКЦИЯ ЗАПИСИ РЕЗУЛЬТАТОВ В CSV ФАЙЛ
// Параметры:
//   filename - путь к файлу
//   array - отсортированный массив
//   len - размер массива
//   shell_stats - статистика сортировки Шелла
//   qsort_stats - статистика qsort
static int write_result_csv(const wchar_t* filename, const long* array, int len,
    const SortStats* shell_stats, const SortStats* qsort_stats)
{
    FILE* fp;
    int i;

    // Открытие файла для записи
    fp = _wfopen(filename, L"w");
    if (fp == NULL) {
        return -1;
    }

    // Запись массива в формате CSV
    for (i = 0; i < len; i++) {
        fprintf(fp, "%ld", array[i]);
        if (i != len - 1) {
            fprintf(fp, ",");
        }
    }
    fprintf(fp, "\n");

    // Запись метаданных и статистики
    fprintf(fp, "size,%d\n", len);
    fprintf(fp, "shell_comparisons,%ld\n", shell_stats->comparisons);
    fprintf(fp, "shell_swaps,%ld\n", shell_stats->swaps);
    fprintf(fp, "shell_time_seconds,%.6f\n", shell_stats->time_seconds);

    if (qsort_stats != NULL) {
        fprintf(fp, "qsort_time_seconds,%.6f\n", qsort_stats->time_seconds);
    }

    fclose(fp);
    return 0;
}

// АЛГОРИТМ СОРТИРОВКИ ШЕЛЛА
// Реализация сортировки Шелла с использованием последовательности Кнута (h = 3*h + 1)
// Параметры:
//   array - сортируемый массив
//   len - размер массива
//   stats - структура для сбора статистики
static void shell_sort(long* array, int len, SortStats* stats)
{
    long h;
    int i, j;
    long temp;
    clock_t start, end;

    // Инициализация счетчиков статистики
    stats->comparisons = 0;
    stats->swaps = 0;

    // Начало замера времени
    start = clock();

    // Вычисление начального шага по формуле Кнута
    h = 1;
    while (h < len / 3) {
        h = 3 * h + 1;
    }

    // Основной цикл сортировки с уменьшающимся шагом
    while (h >= 1) {
        // Сортировка вставками для подмассивов с шагом h
        for (i = h; i < len; i++) {
            temp = array[i];
            j = i;

            // Сдвиг элементов на позицию h влево, если они больше текущего
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

            // Размещение элемента на итоговую позицию
            if (j != i) {
                array[j] = temp;
            }
        }

        // Уменьшение шага в 3 раза
        h /= 3;
    }

    // Завершение замера времени
    end = clock();
    stats->time_seconds = (double)(end - start) / CLOCKS_PER_SEC;
}

// ФУНКЦИЯ СРАВНЕНИЯ ДЛЯ QSORT
// Функция сравнения двух элементов типа long для стандартной qsort
static int compare_long(const void* a, const void* b)
{
    long va = *(const long*)a;
    long vb = *(const long*)b;

    if (va < vb) return -1;
    if (va > vb) return 1;
    return 0;
}

// ИЗМЕРЕНИЕ ПРОИЗВОДИТЕЛЬНОСТИ QSORT
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

// КОПИРОВАНИЕ МАССИВА
// Копирует содержимое одного массива в другой
static void copy_array(const long* src, long* dst, int len)
{
    memcpy(dst, src, (size_t)len * sizeof(long));
}

// ГЕНЕРАЦИЯ ТЕСТОВЫХ ДАННЫХ
// Генерирует массив заданного типа
// Параметры:
//   array - массив для заполнения
//   len - размер массива
//   kind - тип данных (из enum DATA_*)
//   seed - начальное значение для генератора случайных чисел
static void generate_dataset(long* array, int len, int kind, unsigned int seed)
{
    int i;

    srand(seed);

    switch (kind) {
    case DATA_SORTED:
        // Отсортированный массив
        for (i = 0; i < len; i++) array[i] = i;
        break;

    case DATA_REVERSED:
        // Инвертированный массив
        for (i = 0; i < len; i++) array[i] = len - i;
        break;

    case DATA_REPEATED:
        // Массив с повторяющимися элементами (0-9)
        for (i = 0; i < len; i++) array[i] = rand() % 10;
        break;

    case DATA_RANDOM:
    default:
        // Случайный массив
        for (i = 0; i < len; i++) array[i] = rand() % (len * 10 + 1);
        break;
    }
}

// ИДЕНТИФИКАТООРЫ ЭЛЕМЕНТОВ УПРАВЛЕНИЯ GUI
#define ID_BTN_LOAD       101   // Кнопка загрузки CSV
#define ID_BTN_GENERATE   102   // Кнопка генерации данных
#define ID_BTN_SORT       103   // Кнопка сортировки
#define ID_BTN_SAVE       104   // Кнопка сохранения результата
#define ID_EDIT_SIZE      105   // Поле ввода размера массива
#define ID_COMBO_KIND     106   // Выпадающий список типа данных
#define ID_LIST_ARRAY     107   // Список для отображения массива
#define ID_EDIT_LOG       108   // Поле лога и статистики
#define ID_STATIC_SIZE    109   // Метка "Размер:"
#define ID_STATIC_KIND    110   // Метка типа данных

// ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ ПРИЛОЖЕНИЯ
static long  g_array[MAX_SIZE];         // Основной массив данных
static long  g_array_copy[MAX_SIZE];    // Копия массива для сравнения алгоритмов
static int   g_len = 0;                 // Текущий размер массива
static int   g_has_data = 0;            // Флаг наличия данных
static SortStats g_shell_stats;         // Статистика сортировки Шелла
static SortStats g_qsort_stats;         // Статистика qsort
static int   g_has_sorted = 0;          // Флаг выполнения сортировки

static HWND g_hList;                    // Дескриптор списка массива
static HWND g_hLog;                     // Дескриптор поля лога
static HWND g_hEditSize;                // Дескриптор поля размера
static HWND g_hComboKind;               // Дескриптор выпадающего списка

// ФУНКЦИЯ ВЫВОДА СООБЩЕНИЯ В ЛОГ
// Добавляет строку в поле лога
static void log_line(const char* text_utf8)
{
    wchar_t wbuf[1024];
    int len;

    // Преобразование UTF-8 в широкие символы
    len = MultiByteToWideChar(CP_UTF8, 0, text_utf8, -1, wbuf, (int)(sizeof(wbuf) / sizeof(wchar_t)) - 2);
    if (len <= 0) {
        wbuf[0] = L'\0';
        len = 0;
    }
    wbuf[len] = L'\0';

    // Добавление текста в конец поля лога
    {
        int total_len = GetWindowTextLengthW(g_hLog);
        SendMessageW(g_hLog, EM_SETSEL, (WPARAM)total_len, (LPARAM)total_len);
        SendMessageW(g_hLog, EM_REPLACESEL, FALSE, (LPARAM)wbuf);
        SendMessageW(g_hLog, EM_REPLACESEL, FALSE, (LPARAM)L"\r\n");
    }
}

// ФУНКЦИЯ ФОРМАТИРОВАННОГО ВЫВОДА В ЛОГ
static void log_linef(const char* fmt, ...)
{
    char buf[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    log_line(buf);
}

// ПРЕОБРАЗОВАНИЕ UTF-8 В UNICODE
// Конвертирует строку из UTF-8 в широкие символы Windows
static wchar_t* utf8_to_wide(const char* s)
{
    int len = MultiByteToWideChar(CP_UTF8, 0, s, -1, NULL, 0);
    wchar_t* w;

    if (len <= 0) len = 1;
    w = (wchar_t*)malloc((size_t)len * sizeof(wchar_t));
    if (w == NULL) return NULL;
    if (MultiByteToWideChar(CP_UTF8, 0, s, -1, w, len) <= 0) {
        w[0] = L'\0';
    }
    return w;
}

// ФУНКЦИЯ ОТОБРАЖЕНИЯ ДИАЛОГОВОГО ОКНА
// Показывает MessageBox
static int msgbox_utf8(HWND hwnd, const char* text_utf8, const char* title_utf8, UINT type)
{
    wchar_t* wtext = utf8_to_wide(text_utf8);
    wchar_t* wtitle = utf8_to_wide(title_utf8);
    int rc;

    rc = MessageBoxW(hwnd, wtext ? wtext : L"", wtitle ? wtitle : L"", type);

    free(wtext);
    free(wtitle);
    return rc;
}

// ОГРАНИЧЕНИЕ ОТОБРАЖЕНИЯ ЭЛЕМЕНТОВ В СПИСКЕ
#define LIST_DISPLAY_LIMIT 500

// ОБНОВЛЕНИЕ СПИСКА МАССИВА В ИНТЕРФЕЙСЕ
// Обновляет содержимое ListBox, отображая элементы массива
static void refresh_array_list(void)
{
    int i;
    wchar_t buf[64];
    int show;

    // Очистка списка
    SendMessageW(g_hList, LB_RESETCONTENT, 0, 0);

    if (!g_has_data) return;

    // Ограничение количества отображаемых элементов
    show = (g_len < LIST_DISPLAY_LIMIT) ? g_len : LIST_DISPLAY_LIMIT;

    // Добавление элементов в список
    for (i = 0; i < show; i++) {
        swprintf(buf, sizeof(buf) / sizeof(wchar_t), L"[%d] = %ld", i, g_array[i]);
        SendMessageW(g_hList, LB_ADDSTRING, 0, (LPARAM)buf);
    }

    // Сообщение о том, что показаны не все элементы
    if (show < g_len) {
        swprintf(buf, sizeof(buf) / sizeof(wchar_t), L"... показаны первые %d из %d элементов", show, g_len);
        SendMessageW(g_hList, LB_ADDSTRING, 0, (LPARAM)buf);
    }
}

// ОБРАБОТЧИК КНОПКИ "ЗАГРУЗИТЬ CSV"
// Открывает диалог выбора файла и загружает массив из CSV
static void do_load_csv(HWND hwnd)
{
    OPENFILENAMEW ofn;
    wchar_t filename[MAX_PATH] = L"";
    char filename_utf8[MAX_PATH * 2];
    int rc;

    // Инициализация структуры диалога открытия файла
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFilter = L"CSV файлы (*.csv)\0*.csv\0Все файлы (*.*)\0*.*\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
    ofn.lpstrTitle = L"Выберите входной CSV-файл";

    // Показ диалога и проверка результата
    if (!GetOpenFileNameW(&ofn)) return;

    // Преобразование имени файла в UTF-8 для логирования
    WideCharToMultiByte(CP_UTF8, 0, filename, -1, filename_utf8, sizeof(filename_utf8), NULL, NULL);

    // Чтение массива из файла
    rc = read_csv_array(filename, g_array, MAX_SIZE, &g_len);
    if (rc != CSV_OK) {
        char msg[300];
        sprintf(msg, "Ошибка загрузки файла:\n%s", csv_error_message(rc));
        msgbox_utf8(hwnd, msg, "Ошибка", MB_OK | MB_ICONERROR);
        g_has_data = 0;
        return;
    }

    // Обновление состояния и интерфейса
    g_has_data = 1;
    g_has_sorted = 0;
    refresh_array_list();
    log_linef("Загружено из файла: %s", filename_utf8);
    log_linef("Элементов: %d", g_len);
}

// ОБРАБОТЧИК КНОПКИ "СГЕНЕРИРОВАТЬ"
// Генерирует тестовый массив заданного размера и типа
static void do_generate(HWND hwnd)
{
    wchar_t size_text[32];
    int size;
    int kind_index;
    int kind;
    const char* kind_names[] = {
        "отсортированный", "инвертированный", "случайный", "с повторами"
    };

    // Получение размера из поля ввода
    GetWindowTextW(g_hEditSize, size_text, (int)(sizeof(size_text) / sizeof(wchar_t)));
    size = _wtoi(size_text);

    // Проверка корректности размера
    if (size <= 0 || size > MAX_SIZE) {
        char msg[200];
        sprintf(msg, "Введите размер массива от 1 до %d.", MAX_SIZE);
        msgbox_utf8(hwnd, msg, "Некорректный размер", MB_OK | MB_ICONWARNING);
        return;
    }

    // Получение выбранного типа данных
    kind_index = (int)SendMessageW(g_hComboKind, CB_GETCURSEL, 0, 0);
    if (kind_index < 0) kind_index = 2;
    kind = kind_index + 1;

    // Генерация массива
    generate_dataset(g_array, size, kind, (unsigned int)time(NULL));
    g_len = size;
    g_has_data = 1;
    g_has_sorted = 0;

    // Обновление интерфейса
    refresh_array_list();
    log_linef("Сгенерирован массив (%s), размер %d", kind_names[kind_index], size);
}

// ОБРАБОТЧИК КНОПКИ "СОРТИРОВАТЬ"
// Выполняет сортировку Шелла и сравнение с qsort
static void do_sort(HWND hwnd)
{
    // Проверка наличия данных
    if (!g_has_data) {
        msgbox_utf8(hwnd, "Сначала загрузите или сгенерируйте данные.", "Нет данных", MB_OK | MB_ICONWARNING);
        return;
    }

    // Создание копии массива для сравнения алгоритмов
    copy_array(g_array, g_array_copy, g_len);

    // Сортировка массива алгоритмом Шелла
    shell_sort(g_array, g_len, &g_shell_stats);

    // Сортировка копии стандартной qsort
    measure_qsort(g_array_copy, g_len, &g_qsort_stats);
    g_has_sorted = 1;

    // Обновление списка отсортированных данных
    refresh_array_list();

    // Вывод статистики в лог
    log_linef("--- Сортировка выполнена (n = %d) ---", g_len);
    log_linef("Шелл:  сравнений=%ld  перестановок=%ld  время=%.6f с",
        g_shell_stats.comparisons, g_shell_stats.swaps, g_shell_stats.time_seconds);
    log_linef("qsort: время=%.6f с", g_qsort_stats.time_seconds);

    // Вычисление и вывод отношения производительности
    if (g_qsort_stats.time_seconds > 0.0) {
        log_linef("Отношение времени (Шелл / qsort): %.2f",
            g_shell_stats.time_seconds / g_qsort_stats.time_seconds);
    }
}

// ОБРАБОТЧИК КНОПКИ "СОХРАНИТЬ CSV"
// Сохраняет результаты сортировки и статистику в CSV файл
static void do_save_csv(HWND hwnd)
{
    OPENFILENAMEW ofn;
    wchar_t filename[MAX_PATH] = L"result.csv";
    char filename_utf8[MAX_PATH * 2];

    // Проверка наличия данных
    if (!g_has_data) {
        msgbox_utf8(hwnd, "Нет данных для сохранения.", "Нет данных", MB_OK | MB_ICONWARNING);
        return;
    }

    // Инициализация диалога сохранения файла
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFilter = L"CSV файлы (*.csv)\0*.csv\0Все файлы (*.*)\0*.*\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT;
    ofn.lpstrDefExt = L"csv";
    ofn.lpstrTitle = L"Сохранить результат как";

    // Показ диалога
    if (!GetSaveFileNameW(&ofn)) return;

    // Преобразование имени файла
    WideCharToMultiByte(CP_UTF8, 0, filename, -1, filename_utf8, sizeof(filename_utf8), NULL, NULL);

    // Запись результатов в файл
    {
        int rc;

        if (!g_has_sorted) {
            // Если сортировка не выполнялась, записываем только массив
            SortStats empty_stats;
            empty_stats.comparisons = 0;
            empty_stats.swaps = 0;
            empty_stats.time_seconds = 0.0;
            rc = write_result_csv(filename, g_array, g_len, &empty_stats, NULL);
        }
        else {
            // Запись массива и полной статистики
            rc = write_result_csv(filename, g_array, g_len, &g_shell_stats, &g_qsort_stats);
        }

        if (rc != 0) {
            char msg[300];
            sprintf(msg, "Не удалось сохранить файл:\n%s", filename_utf8);
            msgbox_utf8(hwnd, msg, "Ошибка сохранения", MB_OK | MB_ICONERROR);
            return;
        }
    }

    log_linef("Результат сохранён в файл: %s", filename_utf8);
}

// ОБРАБОТЧИК СООБЩЕНИЙ ОКНА
// Главная функция обработки оконных сообщений Windows
LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
    case WM_CREATE: {
        // Создание всех элементов управления при создании окна
        HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
        HWND child;

        // Кнопка загрузки CSV файла
        CreateWindowW(L"BUTTON", L"Загрузить CSV...",
            WS_VISIBLE | WS_CHILD | WS_CLIPSIBLINGS,
            20, 20, 150, 30, hwnd, (HMENU)ID_BTN_LOAD, NULL, NULL);

        // Метка для поля размера
        CreateWindowW(L"STATIC", L"Размер:",
            WS_VISIBLE | WS_CHILD | WS_CLIPSIBLINGS,
            190, 20, 50, 20, hwnd, (HMENU)ID_STATIC_SIZE, NULL, NULL);

        // Поле ввода размера массива
        g_hEditSize = CreateWindowW(L"EDIT", L"1000",
            WS_VISIBLE | WS_CHILD | WS_BORDER | WS_CLIPSIBLINGS,
            245, 18, 70, 24, hwnd, (HMENU)ID_EDIT_SIZE, NULL, NULL);

        // Выпадающий список типа данных
        g_hComboKind = CreateWindowW(L"COMBOBOX", L"",
            WS_VISIBLE | WS_CHILD | WS_CLIPSIBLINGS |
            CBS_DROPDOWNLIST | CBS_HASSTRINGS,
            325, 18, 160, 150, hwnd, (HMENU)ID_COMBO_KIND, NULL, NULL);

        SendMessageW(g_hComboKind, WM_SETFONT, (WPARAM)hFont, FALSE);
        SendMessageW(g_hComboKind, CB_ADDSTRING, 0, (LPARAM)L"Отсортированный");
        SendMessageW(g_hComboKind, CB_ADDSTRING, 0, (LPARAM)L"Инвертированный");
        SendMessageW(g_hComboKind, CB_ADDSTRING, 0, (LPARAM)L"Случайный");
        SendMessageW(g_hComboKind, CB_ADDSTRING, 0, (LPARAM)L"С повторами");
        SendMessageW(g_hComboKind, CB_SETCURSEL, 2, 0);

        // Кнопка генерации данных
        CreateWindowW(L"BUTTON", L"Сгенерировать",
            WS_VISIBLE | WS_CHILD | WS_CLIPSIBLINGS,
            495, 20, 130, 30, hwnd, (HMENU)ID_BTN_GENERATE, NULL, NULL);

        // Кнопка запуска сортировки
        CreateWindowW(L"BUTTON", L"Сортировать (Шелл + qsort)",
            WS_VISIBLE | WS_CHILD | WS_CLIPSIBLINGS,
            20, 60, 220, 32, hwnd, (HMENU)ID_BTN_SORT, NULL, NULL);

        // Кнопка сохранения результатов
        CreateWindowW(L"BUTTON", L"Сохранить CSV...",
            WS_VISIBLE | WS_CHILD | WS_CLIPSIBLINGS,
            250, 60, 150, 32, hwnd, (HMENU)ID_BTN_SAVE, NULL, NULL);

        // Метка списка массива
        CreateWindowW(L"STATIC", L"Массив:",
            WS_VISIBLE | WS_CHILD | WS_CLIPSIBLINGS,
            20, 105, 100, 20, hwnd, NULL, NULL, NULL);

        // Список для отображения элементов массива
        g_hList = CreateWindowW(L"LISTBOX", L"",
            WS_VISIBLE | WS_CHILD | WS_BORDER | WS_VSCROLL | WS_CLIPSIBLINGS | LBS_NOTIFY,
            20, 128, 320, 380, hwnd, (HMENU)ID_LIST_ARRAY, NULL, NULL);

        // Метка поля лога
        CreateWindowW(L"STATIC", L"Лог и статистика:",
            WS_VISIBLE | WS_CHILD | WS_CLIPSIBLINGS,
            360, 105, 150, 20, hwnd, NULL, NULL, NULL);

        // Многострочное поле для вывода лога и статистики
        g_hLog = CreateWindowW(L"EDIT", L"",
            WS_VISIBLE | WS_CHILD | WS_BORDER | WS_VSCROLL | WS_CLIPSIBLINGS |
            ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
            360, 128, 320, 380, hwnd, (HMENU)ID_EDIT_LOG, NULL, NULL);

        // Установка шрифта для всех элементов управления
        child = GetWindow(hwnd, GW_CHILD);
        while (child != NULL) {
            SendMessageW(child, WM_SETFONT, (WPARAM)hFont, TRUE);
            child = GetWindow(child, GW_HWNDNEXT);
        }

        // Начальное сообщение в логе
        log_line("Готово к работе. Загрузите CSV-файл или сгенерируйте тестовые данные.");
        break;
    }

    case WM_COMMAND: {
        // Обработка команд от элементов управления
        int id = LOWORD(wParam);
        switch (id) {
        case ID_BTN_LOAD:     do_load_csv(hwnd); break;
        case ID_BTN_GENERATE: do_generate(hwnd); break;
        case ID_BTN_SORT:     do_sort(hwnd); break;
        case ID_BTN_SAVE:     do_save_csv(hwnd); break;
        }
        break;
    }

    case WM_DESTROY:
        // Завершение работы приложения при закрытии окна
        PostQuitMessage(0);
        break;

    default:
        // Обработка всех остальных сообщений стандартным обработчиком
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    return 0;
}

// ТОЧКА ВХОДА В ПРИЛОЖЕНИЕ
// Главная функция Windows-приложения
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    const wchar_t CLASS_NAME[] = L"ShellSortWindowClass";
    WNDCLASSW wc;
    HWND hwnd;
    MSG msg;

    (void)hPrevInstance;
    (void)lpCmdLine;

    // Включение поддержки DPI для корректного отображения на разных разрешениях
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    // Регистрация класса окна
    ZeroMemory(&wc, sizeof(wc));
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);

    RegisterClassW(&wc);

    // Создание главного окна приложения
    hwnd = CreateWindowExW(
        0,
        CLASS_NAME,
        L"Сортировка Шелла",
        (WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX) | WS_CLIPCHILDREN,
        CW_USEDEFAULT, CW_USEDEFAULT, 720, 560,
        NULL, NULL, hInstance, NULL
    );

    if (hwnd == NULL) return 0;

    // Отображение окна
    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    // Главный цикл обработки сообщений
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return 0;
}