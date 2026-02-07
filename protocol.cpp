#include "protocol.h"
#include <stdio.h>

// Функция потоковой отправки
bool sendAll(SOCKET sock, const char* data, int len) {
    int sent = 0;
    while (sent < len) {
        int result = send(sock, data + sent, len - sent, 0);
        if (result == SOCKET_ERROR || result == 0) {
            return false;
        }
        sent += result;
    }
    return true;
}

// Функция потокового приёма
bool recvAll(SOCKET sock, char* data, int len) {
    int received = 0;
    while (received < len) {
        int result = recv(sock, data + received, len - received, 0);
        if (result == SOCKET_ERROR || result == 0) {
            return false;
        }
        received += result;
    }
    return true;
}

// Обёртки для сообщений протокола
bool sendMessage(SOCKET sock, const void* data, int size) {
    return sendAll(sock, (const char*)data, size);
}

bool receiveMessage(SOCKET sock, void* data, int size) {
    return recvAll(sock, (char*)data, size);
}
