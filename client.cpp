#include <stdio.h>
#include <string.h>
#include <winsock2.h>
#include <windows.h>
#include <fstream>
#include <iostream>
#include <string>
#include "protocol.h"

#pragma comment(lib, "ws2_32.lib")

#define PORT 3333
#define SERVERADDR "127.0.0.1"
#define REQUIRED_SERVER_VERSION 0 // 0 = принять любую версию сервера

// Функция для получения версии сервера
// preferredVersion: 0 = любая, 1 или 2 = только эта версия (при несовпадении сервер вернёт ошибку)
bool getServerVersion(SOCKET sock, unsigned char& version, unsigned char preferredVersion) {
    VersionRequest versionReq;
    versionReq.header.messageType = MSG_VERSION_REQUEST;
    versionReq.header.dataLength = sizeof(VersionRequest) - sizeof(MessageHeader);
    versionReq.clientVersion = preferredVersion;
    
    if (!sendMessage(sock, &versionReq, sizeof(VersionRequest))) {
        printf("Error sending version request\n");
        return false;
    }
    
    MessageHeader respHeader;
    if (!receiveMessage(sock, &respHeader, sizeof(MessageHeader))) {
        printf("Error receiving version response\n");
        return false;
    }
    
    if (respHeader.messageType == MSG_ERROR) {
        ErrorMessage errorMsg;
        errorMsg.header = respHeader;
        if (!receiveMessage(sock, ((char*)&errorMsg) + sizeof(MessageHeader),
                            sizeof(ErrorMessage) - sizeof(MessageHeader))) {
            return false;
        }
        printf("Server error: %s (code: %d)\n", errorMsg.errorText, errorMsg.errorCode);
        return false;
    }
    
    if (respHeader.messageType != MSG_VERSION_RESPONSE) {
        printf("Invalid response type\n");
        return false;
    }
    
    VersionResponse versionResp;
    versionResp.header = respHeader;
    if (!receiveMessage(sock, ((char*)&versionResp) + sizeof(MessageHeader),
                        sizeof(VersionResponse) - sizeof(MessageHeader))) {
        return false;
    }
    
    version = versionResp.version;
    return true;
}

// Функция аутентификации
bool authenticate(SOCKET sock, const char* username, const char* password) {
    AuthRequest authReq;
    authReq.header.messageType = MSG_AUTH_REQUEST;
    authReq.header.dataLength = sizeof(AuthRequest) - sizeof(MessageHeader);
    strncpy_s(authReq.username, username, 63);
    strncpy_s(authReq.password, password, 63);
    authReq.username[63] = '\0';
    authReq.password[63] = '\0';
    
    if (!sendMessage(sock, &authReq, sizeof(AuthRequest))) {
        printf("Error sending auth request\n");
        return false;
    }
    
    AuthResponse authResp;
    if (!receiveMessage(sock, &authResp, sizeof(AuthResponse))) {
        printf("Error receiving auth response\n");
        return false;
    }
    
    if (authResp.header.messageType != MSG_AUTH_RESPONSE) {
        printf("Invalid response type\n");
        return false;
    }
    
    return authResp.success == 1;
}

// Функция регистрации
bool registerUser(SOCKET sock, const char* username, const char* password) {
    RegisterRequest regReq;
    regReq.header.messageType = MSG_REGISTER_REQUEST;
    regReq.header.dataLength = sizeof(RegisterRequest) - sizeof(MessageHeader);
    strncpy_s(regReq.username, username, 63);
    strncpy_s(regReq.password, password, 63);
    regReq.username[63] = '\0';
    regReq.password[63] = '\0';
    
    if (!sendMessage(sock, &regReq, sizeof(RegisterRequest))) {
        printf("Error sending registration request\n");
        return false;
    }
    
    MessageHeader respHeader;
    if (!receiveMessage(sock, &respHeader, sizeof(MessageHeader))) {
        printf("Error receiving registration response\n");
        return false;
    }
    
    if (respHeader.messageType == MSG_ERROR) {
        ErrorMessage errorMsg;
        errorMsg.header = respHeader;
        if (!receiveMessage(sock, ((char*)&errorMsg) + sizeof(MessageHeader),
                            sizeof(ErrorMessage) - sizeof(MessageHeader))) {
            return false;
        }
        printf("Registration failed: %s (code: %d)\n", errorMsg.errorText, errorMsg.errorCode);
        return false;
    }
    
    if (respHeader.messageType != MSG_REGISTER_RESPONSE) {
        printf("Invalid response type\n");
        return false;
    }
    
    RegisterResponse regResp;
    regResp.header = respHeader;
    if (!receiveMessage(sock, ((char*)&regResp) + sizeof(MessageHeader),
                        sizeof(RegisterResponse) - sizeof(MessageHeader))) {
        return false;
    }
    
    return regResp.success == 1;
}

// Функция передачи файла
bool sendFile(SOCKET sock, const char* filename, unsigned char serverVersion) {
    // Определяем тип файла на основе версии сервера
    unsigned char fileType;
    if (serverVersion == SERVER_VERSION_1) {
        fileType = 1;  // Текстовый файл
    } else if (serverVersion == SERVER_VERSION_2) {
        fileType = 2;  // Бинарный файл
    } else {
        printf("Unknown server version: %d\n", serverVersion);
        return false;
    }
    
    // Открываем файл: текстовый режим для версии 1, бинарный для версии 2
    std::ifstream file;
    if (fileType == 1) {  // 1 - текстовые 
        file.open(filename, std::ios::in);
    } else {              // 2 - бинарные
        file.open(filename, std::ios::in | std::ios::binary);
    }
    
    if (!file.is_open()) {
        printf("Error: Cannot open file %s\n", filename);
        return false;
    }
    
    // Определяем размер файла
    file.seekg(0, std::ios::end);
    unsigned int fileSize = (unsigned int)file.tellg();
    file.seekg(0, std::ios::beg);
    
    if (fileSize == 0) {
        printf("Error: File is empty\n");
        file.close();
        return false;
    }
    
    // Читаем файл в буфер
    char* fileData = new char[fileSize];
    file.read(fileData, fileSize);
    file.close();
    
    // Формируем заголовок передачи файла
    FileTransferHeader fileHeader;
    fileHeader.header.messageType = MSG_FILE_TRANSFER;
    fileHeader.header.dataLength = sizeof(FileTransferHeader) - sizeof(MessageHeader);
    fileHeader.fileType = fileType;
    strncpy_s(fileHeader.filename, filename, 255);
    fileHeader.filename[255] = '\0';
    fileHeader.fileSize = fileSize;
    
    // Отправляем заголовок
    if (!sendMessage(sock, &fileHeader, sizeof(FileTransferHeader))) {
        printf("Error sending file header\n");
        delete[] fileData;
        return false;
    }
    
    // Отправляем данные файла
    if (!sendAll(sock, fileData, fileSize)) {
        printf("Error sending file data\n");
        delete[] fileData;
        return false;
    }
    
    printf("File sent: %s (%d bytes)\n", filename, fileSize);
    
    // Ждем ответ от сервера
    MessageHeader responseHeader;
    if (!receiveMessage(sock, &responseHeader, sizeof(MessageHeader))) {
        printf("Error receiving file response header\n");
        delete[] fileData;
        return false;
    }
    
    if (responseHeader.messageType == MSG_ERROR) {
        ErrorMessage errorMsg;
        if (receiveMessage(sock, ((char*)&errorMsg) + sizeof(MessageHeader),
                          sizeof(ErrorMessage) - sizeof(MessageHeader))) {
            printf("Server error: %s (code: %d)\n", errorMsg.errorText, errorMsg.errorCode);
        }
        delete[] fileData;
        return false;
    } else if (responseHeader.messageType == MSG_FILE_RESPONSE) {
        FileResponse fileResp;
        if (receiveMessage(sock, ((char*)&fileResp) + sizeof(MessageHeader),
                          sizeof(FileResponse) - sizeof(MessageHeader))) {
            if (fileResp.success) {
                printf("File transfer successful!\n");
                delete[] fileData;
                return true;
            } else {
                printf("File transfer failed. Error code: %d\n", fileResp.errorCode);
                delete[] fileData;
                return false;
            }
        }
    }
    
    delete[] fileData;
    return false;
}

// Функция обработки ошибок
void handleError(SOCKET sock) {
    MessageHeader header;
    if (recv(sock, (char*)&header, sizeof(MessageHeader), MSG_PEEK) > 0) {
        if (header.messageType == MSG_ERROR) {
            ErrorMessage errorMsg;
            if (receiveMessage(sock, &errorMsg, sizeof(ErrorMessage))) {
                printf("Server error: %s (code: %d)\n", errorMsg.errorText, errorMsg.errorCode);
            }
        }
    }
}

int main(int argc, char* argv[]) {
    char buff[1024];
    printf("TCP FILE TRANSFER CLIENT\n");
    
    // Проверка аргументов командной строки
    if (argc < 2) {
        printf("Usage: %s <filename> [username] [password]\n", argv[0]);
        printf("Example: %s test.txt admin admin123\n", argv[0]);
        return -1;
    }
    
    const char* filename = argv[1];
    const char* username = (argc >= 3) ? argv[2] : "admin";
    const char* password = (argc >= 4) ? argv[3] : "admin123";
    
    // Шаг 1 - Инициализация библиотеки Winsock
    if (WSAStartup(0x202, (WSADATA*)&buff[0])) {
        printf("WSAStart error %d\n", WSAGetLastError());
        return -1;
    }
    
    // Шаг 2 - Создание сокета
    SOCKET my_sock;
    my_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (my_sock < 0) {
        printf("Socket() error %d\n", WSAGetLastError());
        WSACleanup();
        return -1;
    }
    
    // Шаг 3 - Установка соединения
    sockaddr_in dest_addr;
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(PORT);
    
    HOSTENT* hst;
    // Преобразование IP адреса из символьного в сетевой формат
    if (inet_addr(SERVERADDR) != INADDR_NONE) {
        dest_addr.sin_addr.s_addr = inet_addr(SERVERADDR);
    } else {
        // Попытка получить IP адрес по доменному имени сервера
        if (hst = gethostbyname(SERVERADDR)) {
            ((unsigned long*)&dest_addr.sin_addr)[0] = ((unsigned long**)hst->h_addr_list)[0][0];
        } else {
            printf("Invalid address %s\n", SERVERADDR);
            closesocket(my_sock);
            WSACleanup();
            return -1;
        }
    }
    
    // Установка соединения
    if (connect(my_sock, (sockaddr*)&dest_addr, sizeof(dest_addr))) {
        printf("Connect error %d\n", WSAGetLastError());
        closesocket(my_sock);
        WSACleanup();
        return -1;
    }
    
    printf("Connection to %s established successfully\n\n", SERVERADDR);
    
    // Получаем версию сервера
    unsigned char serverVersion;
    printf("Requesting server version...\n");
    if (!getServerVersion(my_sock, serverVersion, REQUIRED_SERVER_VERSION)) {  
        printf("Failed to get server version\n");
        closesocket(my_sock);
        WSACleanup();
        return -1;
    }
    printf("Server version: %d\n", serverVersion);
    
    // Аутентификация
    printf("Authenticating as %s...\n", username);
    if (!authenticate(my_sock, username, password)) {
        printf("Authentication failed. Attempting registration...\n");
        if (!registerUser(my_sock, username, password)) {
            printf("Registration failed. User may already exist.\n");
            printf("Please check your credentials or register with different username.\n");
            closesocket(my_sock);
            WSACleanup();
            return -1;
        }
        printf("Registration successful! Authenticating...\n");
        if (!authenticate(my_sock, username, password)) {
            printf("Authentication failed after registration\n");
            closesocket(my_sock);
            WSACleanup();
            return -1;
        }
    }
    printf("Authentication successful!\n\n");
    
    // Передача файла
    printf("Sending file: %s\n", filename);
    if (!sendFile(my_sock, filename, serverVersion)) {
        printf("File transfer failed\n");
        handleError(my_sock);
        closesocket(my_sock);
        WSACleanup();
        return -1;
    }
    
    printf("\nFile transfer completed successfully!\n");
    
    // Закрытие соединения
    closesocket(my_sock);
    WSACleanup();
    return 0;
}
