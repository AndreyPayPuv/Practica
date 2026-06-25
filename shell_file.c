enum {
    CSV_OK = 0,              
    CSV_ERR_OPEN = -1,       
    CSV_ERR_EMPTY = -2,      
    CSV_ERR_FORMAT = -3,     
    CSV_ERR_TOO_LARGE = -4   
};

enum {
    DATA_SORTED = 1,     
    DATA_REVERSED = 2,   
    DATA_RANDOM = 3,    
    DATA_REPEATED = 4    
};

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

static int read_csv_array(const wchar_t* filename, long* array, int max_len, int* out_len)
{
    FILE* fp;
    char* line = NULL;
    size_t capacity = 4096;
    size_t length = 0;
    int found_nonempty = 0;
    int count = 0;
    char* cursor;

    fp = _wfopen(filename, L"r");
    if (fp == NULL) {
        return CSV_ERR_OPEN;
    }

    line = (char*)malloc(capacity);
    if (line == NULL) {
        fclose(fp);
        return CSV_ERR_OPEN;
    }
    line[0] = '\0';

    while (1) {
        char chunk[4096];
        size_t chunk_len;

        if (fgets(chunk, sizeof(chunk), fp) == NULL) {
            break;
        }

        chunk_len = strlen(chunk);

        if (length + chunk_len + 1 > capacity) {
            size_t new_capacity = capacity * 2;
            char* new_line;

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

        memcpy(line + length, chunk, chunk_len + 1);
        length += chunk_len;

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

    trim_newline(line);
    if (strlen(line) > 0) {
        found_nonempty = 1;
    }

    if (!found_nonempty) {
        free(line);
        return CSV_ERR_EMPTY;
    }

    cursor = line;
    while (1) {
        char* comma = strchr(cursor, ',');
        char field[64];
        size_t field_len;
        char* start, * end, * endptr;
        long value;

        field_len = (comma != NULL) ? (size_t)(comma - cursor) : strlen(cursor);

        if (field_len == 0 || field_len >= sizeof(field)) {
            free(line);
            return CSV_ERR_FORMAT;
        }

        memcpy(field, cursor, field_len);
        field[field_len] = '\0';

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

        endptr = NULL;
        value = strtol(start, &endptr, 10);
        if (endptr == start || *endptr != '\0') {
            free(line);
            return CSV_ERR_FORMAT;
        }

        if (count >= max_len) {
            free(line);
            return CSV_ERR_TOO_LARGE;
        }

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

static int write_result_csv(const wchar_t* filename, const long* array, int len,
    const SortStats* shell_stats, const SortStats* qsort_stats)
{
    FILE* fp;
    int i;

    fp = _wfopen(filename, L"w");
    if (fp == NULL) {
        return -1;
    }

    for (i = 0; i < len; i++) {
        fprintf(fp, "%ld", array[i]);
        if (i != len - 1) {
            fprintf(fp, ",");
        }
    }
    fprintf(fp, "\n");

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

