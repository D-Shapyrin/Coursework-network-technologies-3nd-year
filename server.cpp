#include <stdio.h>
#include <winsock2.h>
#include <windows.h>
#include <string>
#include <fstream>
#include <map>
#include <vector>
#include <sstream>
#include "protocol.h"

#pragma comment(lib, "ws2_32.lib")

#define MY_PORT 3333
#define SERVER_VERSION SERVER_VERSION_2  // Можно изменить на SERVER_VERSION_1 или SERVER_VERSION_2
// SERVER_VERSION_1 Текстовый файл
// SERVER_VERSION_2 Бинарный файл

// Макрос для печати количества активных пользователей
#define PRINTNUSERS if (nclients)\
    printf("%d user(s) online\n", nclients);\
    else printf("No users online\n");

// Прототип функции обслуживания клиента
DWORD WINAPI ServiceToClient(LPVOID client_socket);

// Глобальные переменные
int nclients = 0;
CRITICAL_SECTION cs_users;
CRITICAL_SECTION cs_files;

// Структура для хранения пользователей
struct User {
    std::string username;
    std::string password;
};

// Хранилище пользователей (в реальном приложении - база данных)
std::map<std::string, User> users;

// Разрешенные IP-адреса (диапазон)
struct IPRange {
    unsigned long start;
    unsigned long end;
};

std::vector<IPRange> allowedIPs;

// Инициализация пользователей (для демонстрации)
void initUsers() {
    User admin;
    admin.username = "admin";
    admin.password = "admin123";
    users[admin.username] = admin;
}

// Инициализация разрешенных IP-адресов
void initAllowedIPs() {
    // Разрешаем локальный адрес и диапазон 127.0.0.1 - 127.0.0.255
    IPRange range1;
    range1.start = inet_addr("127.0.0.1");
    range1.end = inet_addr("127.0.0.255");
    allowedIPs.push_back(range1);
    
    // Можно добавить другие диапазоны
    // Например, локальная сеть: 192.168.1.0 - 192.168.1.255
}

// Проверка IP-адреса
bool isIPAllowed(unsigned long ip) {
    for (size_t i = 0; i < allowedIPs.size(); i++) {
        if (ip >= allowedIPs[i].start && ip <= allowedIPs[i].end) {
            return true;
        }
    }
    return false;
}

// Проверка аутентификации
bool authenticateUser(const char* username, const char* password) {
    EnterCriticalSection(&cs_users);
    std::map<std::string, User>::iterator it = users.find(std::string(username));
    if (it != users.end() && it->second.password == std::string(password)) {
        LeaveCriticalSection(&cs_users);
        return true;
    }
    LeaveCriticalSection(&cs_users);
    return false;
}

// Регистрация нового пользователя
bool registerUser(const char* username, const char* password) {
    EnterCriticalSection(&cs_users);
    if (users.find(std::string(username)) != users.end()) {
        LeaveCriticalSection(&cs_users);
        return false;  // Пользователь уже существует
    }
    User newUser;
    newUser.username = std::string(username);
    newUser.password = std::string(password);
    users[newUser.username] = newUser;
    LeaveCriticalSection(&cs_users);
    return true;
}

// Отправка сообщения об ошибке
void sendError(SOCKET sock, unsigned char errorCode, const char* errorText) {
    ErrorMessage errorMsg;
    errorMsg.header.messageType = MSG_ERROR;
    errorMsg.header.dataLength = sizeof(ErrorMessage) - sizeof(MessageHeader);
    errorMsg.errorCode = errorCode;
    strncpy_s(errorMsg.errorText, errorText, 255);
    errorMsg.errorText[255] = '\0';
    sendMessage(sock, &errorMsg, sizeof(ErrorMessage));
}

// Функция обслуживания клиента
DWORD WINAPI ServiceToClient(LPVOID client_socket) {
    SOCKET my_sock = ((SOCKET*)client_socket)[0];
    char buff[64 * 1024];  // Буфер для приема данных
    bool authenticated = false;
    
    printf("Client connected, socket: %d\n", (int)my_sock);
    
    // Цикл обработки сообщений от клиента
    while (true) {
        MessageHeader header;
        
        // Принимаем заголовок сообщения
        if (!receiveMessage(my_sock, &header, sizeof(MessageHeader))) {
            printf("Error receiving message header or client disconnected\n");
            break;
        }
        
        // Обработка различных типов сообщений
        switch (header.messageType) {
            case MSG_VERSION_REQUEST: {
                unsigned char clientVersion = 0;
                if (header.dataLength >= 1) {
                    if (!receiveMessage(my_sock, &clientVersion, 1)) {
                        printf("Error receiving version request body\n");
                        break;
                    }
                }
                if (clientVersion != 0 && clientVersion != SERVER_VERSION) {
                    sendError(my_sock, ERROR_INVALID_VERSION, "Server version not supported");
                    printf("Version mismatch: client wants %d, server is %d\n", 
                           clientVersion, SERVER_VERSION);
                    break;
                }
                VersionResponse versionResp;
                versionResp.header.messageType = MSG_VERSION_RESPONSE;
                versionResp.header.dataLength = sizeof(VersionResponse) - sizeof(MessageHeader);
                versionResp.version = SERVER_VERSION;
                sendMessage(my_sock, &versionResp, sizeof(VersionResponse));
                printf("Version request: sent version %d\n", SERVER_VERSION);
                break;
            }
            
            case MSG_AUTH_REQUEST: {
                AuthRequest authReq;
                if (!receiveMessage(my_sock, ((char*)&authReq) + sizeof(MessageHeader), 
                                   sizeof(AuthRequest) - sizeof(MessageHeader))) {
                    sendError(my_sock, ERROR_INVALID_MESSAGE, "Failed to receive auth data");
                    break;
                }
                
                AuthResponse authResp;
                authResp.header.messageType = MSG_AUTH_RESPONSE;
                authResp.header.dataLength = sizeof(AuthResponse) - sizeof(MessageHeader);
                
                if (authenticateUser(authReq.username, authReq.password)) {
                    authenticated = true;
                    authResp.success = 1;
                    printf("User %s authenticated successfully\n", authReq.username);
                } else {
                    authenticated = false;
                    authResp.success = 0;
                    printf("Authentication failed for user %s\n", authReq.username);
                }
                
                sendMessage(my_sock, &authResp, sizeof(AuthResponse));
                break;
            }
            
            case MSG_REGISTER_REQUEST: {
                RegisterRequest regReq;
                if (!receiveMessage(my_sock, ((char*)&regReq) + sizeof(MessageHeader),
                                   sizeof(RegisterRequest) - sizeof(MessageHeader))) {
                    sendError(my_sock, ERROR_INVALID_MESSAGE, "Failed to receive registration data");
                    break;
                }
                regReq.username[63] = '\0';
                regReq.password[63] = '\0';
                
                if (regReq.username[0] == '\0' || regReq.password[0] == '\0') {
                    sendError(my_sock, ERROR_REGISTRATION_FAILED, "Username and password cannot be empty");
                    printf("Registration failed: empty username or password\n");
                    break;
                }
                
                if (registerUser(regReq.username, regReq.password)) {
                    RegisterResponse regResp;
                    regResp.header.messageType = MSG_REGISTER_RESPONSE;
                    regResp.header.dataLength = sizeof(RegisterResponse) - sizeof(MessageHeader);
                    regResp.success = 1;
                    sendMessage(my_sock, &regResp, sizeof(RegisterResponse));
                    printf("User %s registered successfully\n", regReq.username);
                } else {
                    sendError(my_sock, ERROR_USER_EXISTS, "User already exists");
                    printf("Registration failed for user %s (user already exists)\n", regReq.username);
                }
                break;
            }
            
            case MSG_FILE_TRANSFER: {
                // Сообщение отправляется, только когда приходит MSG_FILE_TRANSFER и при этом authenticated == false.
                if (!authenticated) {
                    sendError(my_sock, ERROR_AUTH_FAILED, "Authentication required");
                    break;
                }
                
                // Получаем информацию о файле
                FileTransferHeader fileHeader;
                if (!receiveMessage(my_sock, ((char*)&fileHeader) + sizeof(MessageHeader),
                                   sizeof(FileTransferHeader) - sizeof(MessageHeader))) {
                    sendError(my_sock, ERROR_INVALID_MESSAGE, "Failed to receive file header");
                    break;
                }
                
                // Проверка соответствия типа файла версии сервера
                if ((SERVER_VERSION == SERVER_VERSION_1 && fileHeader.fileType != 1) ||
                    (SERVER_VERSION == SERVER_VERSION_2 && fileHeader.fileType != 2)) {
                    FileResponse fileResp;
                    fileResp.header.messageType = MSG_FILE_RESPONSE;
                    fileResp.header.dataLength = sizeof(FileResponse) - sizeof(MessageHeader);
                    fileResp.success = 0;
                    fileResp.errorCode = ERROR_FILE_TYPE_MISMATCH;
                    sendMessage(my_sock, &fileResp, sizeof(FileResponse));
                    printf("File type mismatch: server version %d, file type %d\n", 
                           SERVER_VERSION, fileHeader.fileType);
                    break;
                }
                
                // Проверка размера файла
                if (fileHeader.fileSize > sizeof(buff)) {
                    FileResponse fileResp;
                    fileResp.header.messageType = MSG_FILE_RESPONSE;
                    fileResp.header.dataLength = sizeof(FileResponse) - sizeof(MessageHeader);
                    fileResp.success = 0;
                    fileResp.errorCode = ERROR_FILE_TOO_LARGE;
                    sendMessage(my_sock, &fileResp, sizeof(FileResponse));
                    printf("File too large: %d bytes\n", fileHeader.fileSize);
                    break;
                }
                
                // Получаем данные файла
                if (!recvAll(my_sock, buff, fileHeader.fileSize)) {
                    sendError(my_sock, ERROR_INVALID_MESSAGE, "Failed to receive file data");
                    break;
                }
                
                // Сохраняем файл
                EnterCriticalSection(&cs_files);
                std::string filename = std::string("received_") + fileHeader.filename;
                std::ofstream file;
                
                if (fileHeader.fileType == 1) {
                    // Текстовый файл 
                    file.open(filename.c_str(), std::ios::out);
                } else {
                    // Бинарный файл
                    file.open(filename.c_str(), std::ios::out | std::ios::binary);
                }
                
                if (file.is_open()) {
                    file.write(buff, fileHeader.fileSize);
                    file.close();
                    
                    FileResponse fileResp;
                    fileResp.header.messageType = MSG_FILE_RESPONSE;
                    fileResp.header.dataLength = sizeof(FileResponse) - sizeof(MessageHeader);
                    fileResp.success = 1;
                    fileResp.errorCode = 0;
                    sendMessage(my_sock, &fileResp, sizeof(FileResponse));
                    printf("File %s saved successfully (%d bytes)\n", filename.c_str(), fileHeader.fileSize);
                } else {
                    FileResponse fileResp;
                    fileResp.header.messageType = MSG_FILE_RESPONSE;
                    fileResp.header.dataLength = sizeof(FileResponse) - sizeof(MessageHeader);
                    fileResp.success = 0;
                    fileResp.errorCode = ERROR_INVALID_MESSAGE;
                    sendMessage(my_sock, &fileResp, sizeof(FileResponse));
                    printf("Failed to save file %s\n", filename.c_str());
                }
                LeaveCriticalSection(&cs_files);
                break;
            }
            
            default:
                sendError(my_sock, ERROR_INVALID_MESSAGE, "Unknown message type");
                break;
        }
    }
    
    // Клиент отключился
    nclients--;
    printf("- Client disconnected\n");
    PRINTNUSERS;
    closesocket(my_sock);
    return 0;
}

int main(int argc, char* argv[]) {
    char buff[1024];
    printf("TCP FILE TRANSFER SERVER\n");
    printf("Server version: %d\n", SERVER_VERSION);
    
    // Инициализация критических секций
    InitializeCriticalSection(&cs_users);
    InitializeCriticalSection(&cs_files);
    
    // Инициализация пользователей и IP-адресов
    initUsers();
    initAllowedIPs();
    
    // Шаг 1 - Инициализация библиотеки сокетов
    if (WSAStartup(0x0202, (WSADATA*)&buff[0])) {
        printf("Error WSAStartup %d\n", WSAGetLastError());
        DeleteCriticalSection(&cs_users);
        DeleteCriticalSection(&cs_files);
        return -1;
    }
    
    // Шаг 2 - Создание сокета
    SOCKET mysocket;
    if ((mysocket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("Error socket %d\n", WSAGetLastError());
        WSACleanup();
        DeleteCriticalSection(&cs_users);
        DeleteCriticalSection(&cs_files);
        return -1;
    }
    
    // Шаг 3 - Связывание сокета с локальным адресом
    sockaddr_in local_addr;
    local_addr.sin_family = AF_INET;
    local_addr.sin_port = htons(MY_PORT);
    local_addr.sin_addr.s_addr = 0;  // Принимаем подключения на все IP-адреса
    
    if (bind(mysocket, (sockaddr*)&local_addr, sizeof(local_addr))) {
        printf("Error bind %d\n", WSAGetLastError());
        closesocket(mysocket);
        WSACleanup();
        DeleteCriticalSection(&cs_users);
        DeleteCriticalSection(&cs_files);
        return -1;
    }
    
    // Шаг 4 - Ожидание подключений
    if (listen(mysocket, 0x100)) {
        printf("Error listen %d\n", WSAGetLastError());
        closesocket(mysocket);
        WSACleanup();
        DeleteCriticalSection(&cs_users);
        DeleteCriticalSection(&cs_files);
        return -1;
    }
    
    printf("Waiting for connections on port %d...\n", MY_PORT);
    
    // Шаг 5 - Извлечение сообщений из очереди
    SOCKET client_socket;
    sockaddr_in client_addr;
    int client_addr_size = sizeof(client_addr);
    
    // Цикл извлечения запросов на подключение
    while ((client_socket = accept(mysocket, (sockaddr*)&client_addr, &client_addr_size))) {
        // Проверка IP-адреса клиента
        if (!isIPAllowed(client_addr.sin_addr.s_addr)) {
            printf("Connection rejected: IP address %s not allowed\n", 
                   inet_ntoa(client_addr.sin_addr));
            closesocket(client_socket);
            continue;
        }
        
        nclients++;
        
        // Получаем имя хоста
        HOSTENT* hst;
        hst = gethostbyaddr((char*)&client_addr.sin_addr.s_addr, 4, AF_INET);
        
        // Вывод сведений о клиенте
        printf("+ %s [%s] new connection!\n", 
               (hst) ? hst->h_name : "", 
               inet_ntoa(client_addr.sin_addr));
        PRINTNUSERS;
        
        // Вызов нового потока для обслуживания клиента
        DWORD thID;
        CreateThread(NULL, 0, ServiceToClient, &client_socket, 0, &thID);
        
        client_addr_size = sizeof(client_addr);
    }
    
    closesocket(mysocket);
    WSACleanup();
    DeleteCriticalSection(&cs_users);
    DeleteCriticalSection(&cs_files);
    return 0;
}
