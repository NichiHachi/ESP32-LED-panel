#ifndef PINOUT_T_H
#define PINOUT_T_H

// ==========================================
// CONFIGURATION DES BROCHES POUR ESP32-S3
// Basé sur le pinout officiel de la carte
// ==========================================

// --- LED Intégrée (WS2812 RGB) ---
#define PIN_RGB_LED       38    // Souvent connecté à IO38 sur cette variante (ou IO48 selon révision, voir sérigraphie)

// --- Boutons intégrés ---
#define PIN_BUTTON_BOOT   0     // GPIO0 (BOOT)
#define PIN_BUTTON_RST    -1    // Matériel direct (RESET)

// --- Affectations utiles (ex: bus I2C / SPI par défaut) ---
#define PIN_SDA           8     // Exemple standard ou personnalisable
#define PIN_SCL           9

// --- Définitions pour les matrices LED Hub75 (selon la documentation fournie) ---
// Schéma 1 (ESP32-S3-DevKitC-1)
#define R1  37
#define G1   6
#define B1  36
#define R2  35
#define G2   5
#define B2   0
#define A   45
#define B    1
#define C   48
#define D    2
#define E    4
#define CLK 47
#define LAT 38
#define OE  21

#endif // PINOUT_T_H