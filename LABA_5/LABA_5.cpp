#include "framework.h"
#include "LABA_5.h"
#include <windows.h>
#include <string>

#define MAX_LOADSTRING 100

// Глобальные переменные:
HINSTANCE hInst;                                // текущий экземпляр
WCHAR szTitle[MAX_LOADSTRING];                  // Текст строки заголовка
WCHAR szWindowClass[MAX_LOADSTRING];            // имя класса главного окна

// Прототипы функций:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
void AddService(HWND hWnd);
void RemoveService(HWND hWnd);
void StartService(HWND hWnd);
void StopService(HWND hWnd);
void OpenDevice(HWND hWnd);
void CloseDevice(HWND hWnd);
BOOL IsRunAsAdmin();
BOOL EnablePrivilege(LPCWSTR privilege);

HANDLE hDevice = INVALID_HANDLE_VALUE;

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR    lpCmdLine,
    _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    if (!IsRunAsAdmin()) {
        MessageBox(nullptr, L"Программа должна быть запущена с правами администратора.", L"Ошибка", MB_OK | MB_ICONERROR);
        return FALSE;
    }

    // Инициализация глобальных строк
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_LABA5, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    // Выполнить инициализацию приложения:
    if (!InitInstance(hInstance, nCmdShow))
    {
        return FALSE;
    }

    MSG msg;

    // Цикл основного сообщения:
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int)msg.wParam;
}

ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_LABA5));
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = nullptr;
    wcex.lpszClassName = szWindowClass;
    wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
    hInst = hInstance; // Сохранить маркер экземпляра в глобальной переменной

    HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, 0, 600, 400, nullptr, nullptr, hInstance, nullptr);

    if (!hWnd)
    {
        return FALSE;
    }

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    return TRUE;
}

BOOL IsRunAsAdmin()
{
    BOOL isAdmin = FALSE;
    PSID administratorsGroup = nullptr;

    SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;
    if (AllocateAndInitializeSid(&ntAuthority, 2,
        SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS,
        0, 0, 0, 0, 0, 0, &administratorsGroup)) {
        CheckTokenMembership(nullptr, administratorsGroup, &isAdmin);
        FreeSid(administratorsGroup);
    }

    return isAdmin;
}

BOOL EnablePrivilege(LPCWSTR privilege)
{
    HANDLE token;
    TOKEN_PRIVILEGES tp;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &token)) {
        return FALSE;
    }

    if (!LookupPrivilegeValue(nullptr, privilege, &tp.Privileges[0].Luid)) {
        CloseHandle(token);
        return FALSE;
    }

    tp.PrivilegeCount = 1;
    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

    if (!AdjustTokenPrivileges(token, FALSE, &tp, sizeof(TOKEN_PRIVILEGES), nullptr, nullptr)) {
        CloseHandle(token);
        return FALSE;
    }

    CloseHandle(token);
    return GetLastError() == ERROR_SUCCESS;
}

void AddService(HWND hWnd)
{
    wchar_t serviceName[100], symbolicLink[100], filePath[260];

    // Получение данных из полей ввода
    GetWindowText(GetDlgItem(hWnd, 202), serviceName, 100);
    GetWindowText(GetDlgItem(hWnd, 203), symbolicLink, 100);
    GetWindowText(GetDlgItem(hWnd, 201), filePath, 260);

    if (wcslen(serviceName) == 0 || wcslen(filePath) == 0) {
        MessageBox(hWnd, L"Заполните поля 'Service name' и 'File path'.", L"Ошибка", MB_OK | MB_ICONERROR);
        return;
    }

    if (!EnablePrivilege(SE_LOAD_DRIVER_NAME)) {
        MessageBox(hWnd, L"Не удалось установить привилегию SE_LOAD_DRIVER_NAME. Убедитесь, что программа запущена с правами администратора.", L"Ошибка", MB_OK | MB_ICONERROR);
        return;
    }

    // Работа с реестром для создания ключа службы
    HKEY hKey;
    wchar_t registryPath[260];
    swprintf_s(registryPath, L"SYSTEM\\CurrentControlSet\\Services\\%s", serviceName);

    LONG result = RegCreateKeyEx(HKEY_LOCAL_MACHINE, registryPath, 0, nullptr, 0, KEY_WRITE, nullptr, &hKey, nullptr);
    if (result != ERROR_SUCCESS) {
        MessageBox(hWnd, L"Не удалось создать ключ реестра.", L"Ошибка", MB_OK | MB_ICONERROR);
        return;
    }

    // Установка пути к драйверу
    RegSetValueEx(hKey, L"ImagePath", 0, REG_SZ, (const BYTE*)filePath, (DWORD)(wcslen(filePath) + 1) * sizeof(wchar_t));
    RegCloseKey(hKey);

    // Вывод в лог успешного завершения
    HWND hLog = GetDlgItem(hWnd, 204);
    SendMessage(hLog, EM_REPLACESEL, 0, (LPARAM)L"Сервис успешно добавлен.\r\n");
}

void RemoveService(HWND hWnd)
{
    wchar_t serviceName[100];

    // Получение имени службы из поля ввода
    GetWindowText(GetDlgItem(hWnd, 202), serviceName, 100);

    if (wcslen(serviceName) == 0) {
        MessageBox(hWnd, L"Заполните поле 'Service name'.", L"Ошибка", MB_OK | MB_ICONERROR);
        return;
    }

    // Удаление ключа службы из реестра
    wchar_t registryPath[260];
    swprintf_s(registryPath, L"SYSTEM\\CurrentControlSet\\Services\\%s", serviceName);

    LONG result = RegDeleteKey(HKEY_LOCAL_MACHINE, registryPath);
    if (result != ERROR_SUCCESS) {
        MessageBox(hWnd, L"Не удалось удалить службу. Убедитесь, что она остановлена.", L"Ошибка", MB_OK | MB_ICONERROR);
        return;
    }

    // Вывод в лог успешного завершения
    HWND hLog = GetDlgItem(hWnd, 204);
    SendMessage(hLog, EM_REPLACESEL, 0, (LPARAM)L"Сервис успешно удален.\r\n");
}

void StartService(HWND hWnd)
{
    wchar_t serviceName[100];
    GetWindowText(GetDlgItem(hWnd, 202), serviceName, 100);

    if (wcslen(serviceName) == 0) {
        MessageBox(hWnd, L"Заполните поле 'Service name'.", L"Ошибка", MB_OK | MB_ICONERROR);
        return;
    }

    SC_HANDLE hSCManager = OpenSCManager(nullptr, nullptr, SC_MANAGER_ALL_ACCESS);
    if (!hSCManager) {
        MessageBox(hWnd, L"Не удалось открыть диспетчер управления службами.", L"Ошибка", MB_OK | MB_ICONERROR);
        return;
    }

    SC_HANDLE hService = OpenService(hSCManager, serviceName, SERVICE_START);
    if (!hService) {
        MessageBox(hWnd, L"Не удалось открыть службу.", L"Ошибка", MB_OK | MB_ICONERROR);
        CloseServiceHandle(hSCManager);
        return;
    }

    if (!StartService(hService, 0, nullptr)) {
        MessageBox(hWnd, L"Не удалось запустить службу.", L"Ошибка", MB_OK | MB_ICONERROR);
    }
    else {
        HWND hLog = GetDlgItem(hWnd, 204);
        SendMessage(hLog, EM_REPLACESEL, 0, (LPARAM)L"Служба успешно запущена.\r\n");
    }

    CloseServiceHandle(hService);
    CloseServiceHandle(hSCManager);
}

void StopService(HWND hWnd)
{
    wchar_t serviceName[100];
    GetWindowText(GetDlgItem(hWnd, 202), serviceName, 100);

    if (wcslen(serviceName) == 0) {
        MessageBox(hWnd, L"Заполните поле 'Service name'.", L"Ошибка", MB_OK | MB_ICONERROR);
        return;
    }

    SC_HANDLE hSCManager = OpenSCManager(nullptr, nullptr, SC_MANAGER_ALL_ACCESS);
    if (!hSCManager) {
        MessageBox(hWnd, L"Не удалось открыть диспетчер управления службами.", L"Ошибка", MB_OK | MB_ICONERROR);
        return;
    }

    SC_HANDLE hService = OpenService(hSCManager, serviceName, SERVICE_STOP);
    if (!hService) {
        MessageBox(hWnd, L"Не удалось открыть службу.", L"Ошибка", MB_OK | MB_ICONERROR);
        CloseServiceHandle(hSCManager);
        return;
    }

    SERVICE_STATUS status;
    if (!ControlService(hService, SERVICE_CONTROL_STOP, &status)) {
        MessageBox(hWnd, L"Не удалось остановить службу.", L"Ошибка", MB_OK | MB_ICONERROR);
    }
    else {
        HWND hLog = GetDlgItem(hWnd, 204);
        SendMessage(hLog, EM_REPLACESEL, 0, (LPARAM)L"Служба успешно остановлена.\r\n");
    }

    CloseServiceHandle(hService);
    CloseServiceHandle(hSCManager);
}

void OpenDevice(HWND hWnd)
{
    wchar_t symbolicLink[100];
    GetWindowText(GetDlgItem(hWnd, 203), symbolicLink, 100);

    if (wcslen(symbolicLink) == 0) {
        MessageBox(hWnd, L"Заполните поле 'Symbolic Link Name'.", L"Ошибка", MB_OK | MB_ICONERROR);
        return;
    }

    wchar_t devicePath[260];
    swprintf_s(devicePath, L"\\\\.\\%s", symbolicLink);

    hDevice = CreateFile(devicePath, GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);

    if (hDevice == INVALID_HANDLE_VALUE) {
        MessageBox(hWnd, L"Не удалось открыть устройство.", L"Ошибка", MB_OK | MB_ICONERROR);
    }
    else {
        HWND hLog = GetDlgItem(hWnd, 204);
        SendMessage(hLog, EM_REPLACESEL, 0, (LPARAM)L"Устройство успешно открыто.\r\n");
    }
}

void CloseDevice(HWND hWnd)
{
    if (hDevice != INVALID_HANDLE_VALUE) {
        CloseHandle(hDevice);
        hDevice = INVALID_HANDLE_VALUE;

        HWND hLog = GetDlgItem(hWnd, 204);
        SendMessage(hLog, EM_REPLACESEL, 0, (LPARAM)L"Устройство успешно закрыто.\r\n");
    }
}


LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_CREATE:
    {
        // Поле "File path"
        CreateWindowW(L"STATIC", L"File path:",
            WS_CHILD | WS_VISIBLE,
            10, 10, 100, 20, hWnd,
            nullptr, hInst, nullptr);
        CreateWindowW(L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
            110, 10, 200, 20, hWnd,
            (HMENU)201, hInst, nullptr);

        // Поле "Service name"
        CreateWindowW(L"STATIC", L"Service name:",
            WS_CHILD | WS_VISIBLE,
            10, 40, 100, 20, hWnd,
            nullptr, hInst, nullptr);
        CreateWindowW(L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
            110, 40, 200, 20, hWnd,
            (HMENU)202, hInst, nullptr);

        // Поле "Symbolic Link Name"
        CreateWindowW(L"STATIC", L"Symbolic Link Name:",
            WS_CHILD | WS_VISIBLE,
            10, 70, 120, 20, hWnd,
            nullptr, hInst, nullptr);
        CreateWindowW(L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
            130, 70, 180, 20, hWnd,
            (HMENU)203, hInst, nullptr);

        // Кнопки
        const wchar_t* buttonLabels[] = { L"Add", L"Remove", L"Start", L"Stop", L"Open", L"Close" };
        for (int i = 0; i < 6; ++i) {
            CreateWindowW(L"BUTTON", buttonLabels[i],
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                10, 100 + i * 40, 100, 30, hWnd,
                (HMENU)(301 + i), hInst, nullptr);
        }

        // Поле вывода лога
        CreateWindowW(L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE | WS_VSCROLL | ES_AUTOVSCROLL,
            130, 150, 440, 200, hWnd,
            (HMENU)204, hInst, nullptr);

        break;
    }
    case WM_COMMAND:
    {
        int wmId = LOWORD(wParam);

        // Обработка кнопок
        switch (wmId)
        {
        case 301: // Add
            AddService(hWnd);
            break;
        case 302: // Remove
            RemoveService(hWnd);
            break;
        case 303: // Start
            StartService(hWnd);
            break;
        case 304: // Stop
            StopService(hWnd);
            break;
        case 305: // Open
            OpenDevice(hWnd);
            break;
        case 306: // Close
            CloseDevice(hWnd);
            break;
        default:
            break;
        }
        break;
    }
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}