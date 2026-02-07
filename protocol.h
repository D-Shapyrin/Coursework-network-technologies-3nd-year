#ifndef PROTOCOL_H
#define PROTOCOL_H

// Включаем Windows заголовки с защитой от конфликтов
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <windows.h>
#undef ERROR_FILE_TOO_LARGE
#undef ERROR_INVALID_MESSAGE
#undef ERROR_USER_EXISTS
#include <string>

// Версии сервера
#define SERVER_VERSION_1 1  // Текстовые файлы
#define SERVER_VERSION_2 2  // Бинарные файлы

// Типы сообщений
#define MSG_VERSION_REQUEST 0x01
#define MSG_VERSION_RESPONSE 0x02
#define MSG_AUTH_REQUEST 0x03
#define MSG_AUTH_RESPONSE 0x04
#define MSG_REGISTER_REQUEST 0x05
#define MSG_REGISTER_RESPONSE 0x06
#define MSG_FILE_TRANSFER 0x07
#define MSG_FILE_RESPONSE 0x08
#define MSG_ERROR 0xFF

// Коды ошибок (используем префикс FT_ чтобы избежать конфликтов с Windows API)
#define FT_ERROR_INVALID_VERSION 0x01
#define FT_ERROR_AUTH_FAILED 0x02
#define FT_ERROR_IP_NOT_ALLOWED 0x03
#define FT_ERROR_FILE_TYPE_MISMATCH 0x04
#define FT_ERROR_FILE_TOO_LARGE 0x05
#define FT_ERROR_INVALID_MESSAGE 0x06
#define FT_ERROR_USER_EXISTS 0x07
#define FT_ERROR_REGISTRATION_FAILED 0x08

// Алиасы для обратной совместимости
#define ERROR_INVALID_VERSION FT_ERROR_INVALID_VERSION
#define ERROR_AUTH_FAILED FT_ERROR_AUTH_FAILED
#define ERROR_IP_NOT_ALLOWED FT_ERROR_IP_NOT_ALLOWED
#define ERROR_FILE_TYPE_MISMATCH FT_ERROR_FILE_TYPE_MISMATCH
#define ERROR_FILE_TOO_LARGE FT_ERROR_FILE_TOO_LARGE
#define ERROR_INVALID_MESSAGE FT_ERROR_INVALID_MESSAGE
#define ERROR_USER_EXISTS FT_ERROR_USER_EXISTS
#define ERROR_REGISTRATION_FAILED FT_ERROR_REGISTRATION_FAILED

// Структура заголовка сообщения
// С pack(1) — без padding:
#pragma pack(push, 1)
struct MessageHeader {
    unsigned char messageType;   // 1 байт, offset 0
    unsigned int dataLength;     // 4 байта, offset 1
};  // sizeof = 5

// Структура запроса версии
struct VersionRequest {
    MessageHeader header;
    unsigned char clientVersion;  // 0 = любая, 1 или 2 = только эта версия
};
#pragma pack(pop)

// Структура ответа версии
struct VersionResponse {
    MessageHeader header;
    unsigned char version;
};

// Структура запроса аутентификации
struct AuthRequest {
    MessageHeader header;
    char username[64];
    char password[64];
};

// Структура ответа аутентификации
struct AuthResponse {
    MessageHeader header;
    unsigned char success;  // 1 - успех, 0 - ошибка
};

// Структура запроса регистрации
struct RegisterRequest {
    MessageHeader header;
    char username[64];
    char password[64];
};

// Структура ответа регистрации
struct RegisterResponse {
    MessageHeader header;
    unsigned char success;  // 1 - успех, 0 - ошибка
};

// Структура передачи файла
struct FileTransferHeader {
    MessageHeader header;
    unsigned char fileType;  // 1 - текст, 2 - бинарный
    char filename[256];
    unsigned int fileSize;
};

// Структура ответа на передачу файла
struct FileResponse {
    MessageHeader header;
    unsigned char success;  // 1 - успех, 0 - ошибка
    unsigned char errorCode;  // Код ошибки, если success = 0
};

// Структура сообщения об ошибке
struct ErrorMessage {
    MessageHeader header;
    unsigned char errorCode;
    char errorText[256];
};

// Вспомогательные функции для отправки/приема сообщений
bool sendMessage(SOCKET sock, const void* data, int size);
bool receiveMessage(SOCKET sock, void* data, int size);
bool sendAll(SOCKET sock, const char* data, int len);
bool recvAll(SOCKET sock, char* data, int len);

#endif // PROTOCOL_H
