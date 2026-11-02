#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoWebsockets.h>
#include "pinout.h"
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>

using namespace websockets;

const char *ssid = "SFR_9B60";
const char *password = "yuwx4hwr33bfqh2drr55";

const char *websockets_server_host = "192.168.1.97";
const uint16_t websockets_server_port = 18080;
const char *websockets_server_path = "/esp/ws";

const int PANEL_RES_X = 32;
const int PANEL_RES_Y = 32;
const int TOTAL_PIXELS = PANEL_RES_X * PANEL_RES_Y;
const int EXPECTED_BYTES = TOTAL_PIXELS * 3;

// Objet d'affichage HUB75
MatrixPanel_I2S_DMA *dma_display = nullptr;

// Client WebSocket
WebsocketsClient webSocket;

void onMessageCallback(WebsocketsMessage message) {
    if (message.isBinary()) {
        const uint8_t *payload = (const uint8_t *) message.c_str();
        std::size_t length = message.length();

        Serial.printf("Frame binaire reçue ! Taille : %d octets (Attendu : %d)\n", length, EXPECTED_BYTES);

        // Vérification de la taille de la trame reçue
        if (length >= EXPECTED_BYTES) {
            int idx = 0;
            for (int y = 0; y < PANEL_RES_Y; y++) {
                for (int x = 0; x < PANEL_RES_X; x++) {
                    uint8_t r = payload[idx++];
                    uint8_t g = payload[idx++];
                    uint8_t b = payload[idx++];

                    uint16_t color = dma_display->color565(r, g, b);
                    dma_display->drawPixel(x, y, color);
                }
            }
            // Envoi de l'acquittement "ACK" au serveur pour la frame suivante
            webSocket.send("ACK");
        } else {
            Serial.println("Erreur : Taille de trame reçue inférieure à EXPECTED_BYTES !");
        }
    } else {
        // Si le serveur envoie un message texte (ex: autre chose que binaire)
        Serial.print("Message texte reçu : ");
        Serial.println(message.data());
    }
}

void onEventsCallback(WebsocketsEvent event, String data) {
    if (event == WebsocketsEvent::ConnectionOpened) {
        Serial.println("Connecté au serveur WebSocket !");
    } else if (event == WebsocketsEvent::ConnectionClosed) {
        Serial.println("Connexion WebSocket fermée.");
    }
}

void setup() {
    Serial.begin(115200);
    Serial.println("\nDémarrage de l'ESP32-S3...");

    // 1. Configuration et initialisation de la matrice LED HUB75 (Correction structure CFG)
    HUB75_I2S_CFG mxconfig(PANEL_RES_X, PANEL_RES_Y, 1);

    // Association des broches depuis pinout.h
    mxconfig.gpio.r1 = R1;
    mxconfig.gpio.g1 = G1;
    mxconfig.gpio.b1 = B1;
    mxconfig.gpio.r2 = R2;
    mxconfig.gpio.g2 = G2;
    mxconfig.gpio.b2 = B2;
    mxconfig.gpio.a = A;
    mxconfig.gpio.b = B;
    mxconfig.gpio.c = C;
    mxconfig.gpio.d = D;
    mxconfig.gpio.e = E;
    mxconfig.gpio.lat = LAT;
    mxconfig.gpio.oe = OE;
    mxconfig.gpio.clk = CLK;

    dma_display = new MatrixPanel_I2S_DMA(mxconfig);
    dma_display->begin();
    dma_display->setBrightness8(128); // Luminosité de 0 à 255
    dma_display->fillScreen(0); // Remplacement de clear() par fillScreen(0)

    // 2. Connexion au Wi-Fi
    WiFi.begin(ssid, password);
    Serial.print("Connexion au Wi-Fi");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWi-Fi connecté ! Adresse IP : ");
    Serial.println(WiFi.localIP());

    // 3. Configuration du WebSocket
    webSocket.onMessage(onMessageCallback);
    webSocket.onEvent(onEventsCallback);

    // Connexion au serveur WebSocket
    Serial.printf("Tentative de connexion à ws://%s:%d%s\n", websockets_server_host, websockets_server_port,
                  websockets_server_path);
    webSocket.connect(websockets_server_host, websockets_server_port, websockets_server_path);
    dma_display->begin();
    dma_display->setBrightness8(128);
    dma_display->fillScreen(dma_display->color565(255, 0, 0)); // Test en fin de setup
}

void loop() {
    // Maintien de la connexion WebSocket active
    if (webSocket.available()) {
        webSocket.poll();
    } else {
        // Tentative de reconnexion si la connexion est perdue
        static unsigned long lastReconnectAttempt = 0;
        if (millis() - lastReconnectAttempt > 5000) {
            lastReconnectAttempt = millis();
            Serial.println("Reconnexion au WebSocket...");
            webSocket.connect(websockets_server_host, websockets_server_port, websockets_server_path);
        }
    }
}
