#define ID_BTN_LOAD       101
#define ID_BTN_GENERATE   102
#define ID_BTN_SORT       103
#define ID_BTN_SAVE       104
#define ID_EDIT_SIZE      105
#define ID_COMBO_KIND     106
#define ID_LIST_ARRAY     107
#define ID_EDIT_LOG       108
#define ID_STATIC_SIZE    109
#define ID_STATIC_KIND    110

static long  g_array[MAX_SIZE];
static long  g_array_copy[MAX_SIZE];
static int   g_len = 0;
static int   g_has_data = 0;
static SortStats g_shell_stats;
static SortStats g_qsort_stats;
static int   g_has_sorted = 0;

static HWND g_hList;
static HWND g_hLog;
static HWND g_hEditSize;
static HWND g_hComboKind;

static void log_line(const char* text_utf8)
{
    wchar_t wbuf[1024];
    int len;

    len = MultiByteToWideChar(CP_UTF8, 0, text_utf8, -1, wbuf, (int)(sizeof(wbuf) / sizeof(wchar_t)) - 2);
    if (len <= 0) {
        wbuf[0] = L'\0';
        len = 0;
    }
    wbuf[len] = L'\0';

    {
        int total_len = GetWindowTextLengthW(g_hLog);
        SendMessageW(g_hLog, EM_SETSEL, (WPARAM)total_len, (LPARAM)total_len);
        SendMessageW(g_hLog, EM_REPLACESEL, FALSE, (LPARAM)wbuf);
        SendMessageW(g_hLog, EM_REPLACESEL, FALSE, (LPARAM)L"\r\n");
    }
}

static void log_linef(const char* fmt, ...)
{
    char buf[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    log_line(buf);
}

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

#define LIST_DISPLAY_LIMIT 500

static void refresh_array_list(void)
{
    int i;
    wchar_t buf[64];
    int show;

    SendMessageW(g_hList, LB_RESETCONTENT, 0, 0);

    if (!g_has_data) return;

    show = (g_len < LIST_DISPLAY_LIMIT) ? g_len : LIST_DISPLAY_LIMIT;

    for (i = 0; i < show; i++) {
        swprintf(buf, sizeof(buf) / sizeof(wchar_t), L"[%d] = %ld", i, g_array[i]);
        SendMessageW(g_hList, LB_ADDSTRING, 0, (LPARAM)buf);
    }

    if (show < g_len) {
        swprintf(buf, sizeof(buf) / sizeof(wchar_t), L"... показаны первые %d из %d элементов", show, g_len);
        SendMessageW(g_hList, LB_ADDSTRING, 0, (LPARAM)buf);
    }
}

static void do_load_csv(HWND hwnd)
{
    OPENFILENAMEW ofn;
    wchar_t filename[MAX_PATH] = L"";
    char filename_utf8[MAX_PATH * 2];
    int rc;

    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFilter = L"CSV файлы (*.csv)\0*.csv\0Все файлы (*.*)\0*.*\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
    ofn.lpstrTitle = L"Выберите входной CSV-файл";

    if (!GetOpenFileNameW(&ofn)) return;

    WideCharToMultiByte(CP_UTF8, 0, filename, -1, filename_utf8, sizeof(filename_utf8), NULL, NULL);

    rc = read_csv_array(filename, g_array, MAX_SIZE, &g_len);
    if (rc != CSV_OK) {
        char msg[300];
        sprintf(msg, "Ошибка загрузки файла:\n%s", csv_error_message(rc));
        msgbox_utf8(hwnd, msg, "Ошибка", MB_OK | MB_ICONERROR);
        g_has_data = 0;
        return;
    }

    g_has_data = 1;
    g_has_sorted = 0;
    refresh_array_list();
    log_linef("Загружено из файла: %s", filename_utf8);
    log_linef("Элементов: %d", g_len);
}

static void do_generate(HWND hwnd)
{
    wchar_t size_text[32];
    int size;
    int kind_index;
    int kind;
    const char* kind_names[] = {
        "отсортированный", "инвертированный", "случайный", "с повторами"
    };

    GetWindowTextW(g_hEditSize, size_text, (int)(sizeof(size_text) / sizeof(wchar_t)));
    size = _wtoi(size_text);

    if (size <= 0 || size > MAX_SIZE) {
        char msg[200];
        sprintf(msg, "Введите размер массива от 1 до %d.", MAX_SIZE);
        msgbox_utf8(hwnd, msg, "Некорректный размер", MB_OK | MB_ICONWARNING);
        return;
    }

    kind_index = (int)SendMessageW(g_hComboKind, CB_GETCURSEL, 0, 0);
    if (kind_index < 0) kind_index = 2;
    kind = kind_index + 1;

    generate_dataset(g_array, size, kind, (unsigned int)time(NULL));
    g_len = size;
    g_has_data = 1;
    g_has_sorted = 0;

    refresh_array_list();
    log_linef("Сгенерирован массив (%s), размер %d", kind_names[kind_index], size);
}

static void do_sort(HWND hwnd)
{
    if (!g_has_data) {
        msgbox_utf8(hwnd, "Сначала загрузите или сгенерируйте данные.", "Нет данных", MB_OK | MB_ICONWARNING);
        return;
    }

    copy_array(g_array, g_array_copy, g_len);

    shell_sort(g_array, g_len, &g_shell_stats);

    measure_qsort(g_array_copy, g_len, &g_qsort_stats);
    g_has_sorted = 1;

    refresh_array_list();

    log_linef("--- Сортировка выполнена (n = %d) ---", g_len);
    log_linef("Шелл:  сравнений=%ld  перестановок=%ld  время=%.6f с",
        g_shell_stats.comparisons, g_shell_stats.swaps, g_shell_stats.time_seconds);
    log_linef("qsort: время=%.6f с", g_qsort_stats.time_seconds);

    if (g_qsort_stats.time_seconds > 0.0) {
        log_linef("Отношение времени (Шелл / qsort): %.2f",
            g_shell_stats.time_seconds / g_qsort_stats.time_seconds);
    }
}

static void do_save_csv(HWND hwnd)
{
    OPENFILENAMEW ofn;
    wchar_t filename[MAX_PATH] = L"result.csv";
    char filename_utf8[MAX_PATH * 2];

    if (!g_has_data) {
        msgbox_utf8(hwnd, "Нет данных для сохранения.", "Нет данных", MB_OK | MB_ICONWARNING);
        return;
    }

    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFilter = L"CSV файлы (*.csv)\0*.csv\0Все файлы (*.*)\0*.*\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT;
    ofn.lpstrDefExt = L"csv";
    ofn.lpstrTitle = L"Сохранить результат как";

    if (!GetSaveFileNameW(&ofn)) return;

    WideCharToMultiByte(CP_UTF8, 0, filename, -1, filename_utf8, sizeof(filename_utf8), NULL, NULL);

    {
        int rc;

        if (!g_has_sorted) {
            SortStats empty_stats;
            empty_stats.comparisons = 0;
            empty_stats.swaps = 0;
            empty_stats.time_seconds = 0.0;
            rc = write_result_csv(filename, g_array, g_len, &empty_stats, NULL);
        }
        else {
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

LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
    case WM_CREATE: {
        HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
        HWND child;

        CreateWindowW(L"BUTTON", L"Загрузить CSV...",
            WS_VISIBLE | WS_CHILD | WS_CLIPSIBLINGS,
            20, 20, 150, 30, hwnd, (HMENU)ID_BTN_LOAD, NULL, NULL);

        CreateWindowW(L"STATIC", L"Размер:",
            WS_VISIBLE | WS_CHILD | WS_CLIPSIBLINGS,
            190, 20, 50, 20, hwnd, (HMENU)ID_STATIC_SIZE, NULL, NULL);

        g_hEditSize = CreateWindowW(L"EDIT", L"1000",
            WS_VISIBLE | WS_CHILD | WS_BORDER | WS_CLIPSIBLINGS,
            245, 18, 70, 24, hwnd, (HMENU)ID_EDIT_SIZE, NULL, NULL);

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

        CreateWindowW(L"BUTTON", L"Сгенерировать",
            WS_VISIBLE | WS_CHILD | WS_CLIPSIBLINGS,
            495, 20, 130, 30, hwnd, (HMENU)ID_BTN_GENERATE, NULL, NULL);

        CreateWindowW(L"BUTTON", L"Сортировать (Шелл + qsort)",
            WS_VISIBLE | WS_CHILD | WS_CLIPSIBLINGS,
            20, 60, 220, 32, hwnd, (HMENU)ID_BTN_SORT, NULL, NULL);

        CreateWindowW(L"BUTTON", L"Сохранить CSV...",
            WS_VISIBLE | WS_CHILD | WS_CLIPSIBLINGS,
            250, 60, 150, 32, hwnd, (HMENU)ID_BTN_SAVE, NULL, NULL);

        CreateWindowW(L"STATIC", L"Массив:",
            WS_VISIBLE | WS_CHILD | WS_CLIPSIBLINGS,
            20, 105, 100, 20, hwnd, NULL, NULL, NULL);

        g_hList = CreateWindowW(L"LISTBOX", L"",
            WS_VISIBLE | WS_CHILD | WS_BORDER | WS_VSCROLL | WS_CLIPSIBLINGS | LBS_NOTIFY,
            20, 128, 320, 380, hwnd, (HMENU)ID_LIST_ARRAY, NULL, NULL);

        CreateWindowW(L"STATIC", L"Лог и статистика:",
            WS_VISIBLE | WS_CHILD | WS_CLIPSIBLINGS,
            360, 105, 150, 20, hwnd, NULL, NULL, NULL);

        g_hLog = CreateWindowW(L"EDIT", L"",
            WS_VISIBLE | WS_CHILD | WS_BORDER | WS_VSCROLL | WS_CLIPSIBLINGS |
            ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
            360, 128, 320, 380, hwnd, (HMENU)ID_EDIT_LOG, NULL, NULL);

        child = GetWindow(hwnd, GW_CHILD);
        while (child != NULL) {
            SendMessageW(child, WM_SETFONT, (WPARAM)hFont, TRUE);
            child = GetWindow(child, GW_HWNDNEXT);
        }

        log_line("Готово к работе. Загрузите CSV-файл или сгенерируйте тестовые данные.");
        break;
    }

    case WM_COMMAND: {
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
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    const wchar_t CLASS_NAME[] = L"ShellSortWindowClass";
    WNDCLASSW wc;
    HWND hwnd;
    MSG msg;

    (void)hPrevInstance;
    (void)lpCmdLine;

    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    ZeroMemory(&wc, sizeof(wc));
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);

    RegisterClassW(&wc);

    hwnd = CreateWindowExW(
        0,
        CLASS_NAME,
        L"Сортировка Шелла",
        (WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX) | WS_CLIPCHILDREN,
        CW_USEDEFAULT, CW_USEDEFAULT, 720, 560,
        NULL, NULL, hInstance, NULL
    );

    if (hwnd == NULL) return 0;

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}