#include <WiFi.h>
#include <WebSocketsClient.h>

#include "secrets.h"

const char* serverIP = "192.168.15.8";
const int serverPort = 3000;

const int led1 = 25;
const int led2 = 26;
const int led3 = 27;

WebSocketsClient webSocket;

void apagarLeds() {
    digitalWrite(led1, LOW);
    digitalWrite(led2, LOW);
    digitalWrite(led3, LOW);
}

void acenderLed(int numero) {
    apagarLeds();

    switch (numero) {
        case 1:
            digitalWrite(led1, HIGH);
            break;
        case 2:
            digitalWrite(led2, HIGH);
            break;
        case 3:
            digitalWrite(led3, HIGH);
            break;
    }
}

void webSocketEvent(WStype_t type, uint8_t* payload, size_t length) {
    switch (type) {
        case WStype_DISCONNECTED:
            Serial.println("Desconectado do servidor!");
            apagarLeds();
            break;

        case WStype_CONNECTED:
            Serial.println("Conectado ao servidor!");
            break;

        case WStype_TEXT:
            if (length == 1 && payload[0] >= '1' && payload[0] <= '3') {
                int estacao = payload[0] - '0';
                Serial.print("Próxima estação: ");
                Serial.println(estacao);
                acenderLed(estacao);
            } else {
                Serial.println("Mensagem invalida.");
            }
            break;

        default:
            break;
    }
}

void setup() {
    Serial.begin(115200);

    pinMode(led1, OUTPUT);
    pinMode(led2, OUTPUT);
    pinMode(led3, OUTPUT);
    apagarLeds();

    WiFi.begin(ssid, password);
    Serial.print("Conectando ao Wi-Fi");

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    Serial.println("\nWi-Fi conectado!");

    Serial.print("IP do ESP32: ");
    Serial.println(WiFi.localIP());

    Serial.println("\nConectando ao servidor WebSocket...");

    webSocket.begin(serverIP, serverPort, "/");
    webSocket.onEvent(webSocketEvent);
    webSocket.setReconnectInterval(5000);
}

void loop() {
    webSocket.loop();
}
