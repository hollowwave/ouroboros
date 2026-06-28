#pragma once

// ─────────────────────────────────────────
//  Display — ST7735 1.44" (128x128)
// ─────────────────────────────────────────
#define PIN_TFT_CS    5
#define PIN_TFT_DC    2
#define PIN_TFT_RST   4
#define PIN_TFT_MOSI  23
#define PIN_TFT_SCK   18
#define PIN_TFT_BL    -1

#define SCREEN_W      128
#define SCREEN_H      128

// ─────────────────────────────────────────
//  CC1101
// ─────────────────────────────────────────
#define PIN_CC1101_CS    15
#define PIN_CC1101_MOSI  23
#define PIN_CC1101_MISO  19
#define PIN_CC1101_SCK   18
#define PIN_CC1101_GDO0  22
#define PIN_CC1101_GDO2  21

// ─────────────────────────────────────────
//  Buttons
// ─────────────────────────────────────────
#define PIN_BTN_UP      12
#define PIN_BTN_DOWN    13
#define PIN_BTN_SELECT  14
#define PIN_BTN_BACK    27
#define BTN_DEBOUNCE_MS 50

// ─────────────────────────────────────────
//  UI Layout
// ─────────────────────────────────────────
#define STATUSBAR_H     13
#define MENU_ITEM_H     14
#define MENU_MAX_ITEMS  7

// ─────────────────────────────────────────
//  OUROBOROS Color Palette (RGB565)
//
//  Theme: Red on Black
//  Aggressive, sharp, tool-like
// ─────────────────────────────────────────
#define CLR_BG          0x0000   // Pure black
#define CLR_TEXT        0xFFFF   // White — primary labels
#define CLR_DIM         0x8410   // Mid grey — metadata, hints
#define CLR_SUBTLE      0x2104   // Very dark grey — row backgrounds

// Red family
#define CLR_ACCENT      0xF800   // Pure red — selections, highlights
#define CLR_ACCENT_DIM  0x7800   // Dark red — inactive borders, secondary
#define CLR_ACCENT_GLOW 0xF8C0   // Red-orange — glow/pulse effect

// Status colors
#define CLR_WARN        0xFFE0   // Yellow — warnings
#define CLR_OK          0x07E0   // Green — success / fixed code
#define CLR_DANGER      0xF800   // Red — attacks (same as accent intentionally)
#define CLR_INFO        0x051F   // Blue — neutral info

// ─────────────────────────────────────────
//  Ouroboros Animation
// ─────────────────────────────────────────
#define OURO_SEGMENTS   8        // Snake body segments
#define OURO_RADIUS     18       // Pixel radius of the circle
#define OURO_CX         64       // Center X
#define OURO_CY         54       // Center Y (splash screen)
#define OURO_ANIM_MS    80       // Ms per frame

// ─────────────────────────────────────────
//  Serial
// ─────────────────────────────────────────
#define SERIAL_BAUD     115200
#define DEBUG_ENABLED   true
