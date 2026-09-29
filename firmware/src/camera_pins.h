#pragma once

// Распиновка камеры для Freenove ESP32-S3-WROOM CAM.
// ВАЖНО: сверьте с документацией именно вашей ревизии платы на freenove.com —
// Freenove выпускал несколько похожих плат (WROOM / N16R8 / N8R8), распиновка
// камеры у них совпадает, но лучше сверить перед первой прошивкой, чтобы не
// ловить "camera init failed" из-за банально неверного пина.

#define CAM_PIN_PWDN  -1
#define CAM_PIN_RESET -1
#define CAM_PIN_XCLK  15
#define CAM_PIN_SIOD   4
#define CAM_PIN_SIOC   5

#define CAM_PIN_D7    16
#define CAM_PIN_D6    17
#define CAM_PIN_D5    18
#define CAM_PIN_D4    12
#define CAM_PIN_D3    10
#define CAM_PIN_D2     8
#define CAM_PIN_D1     9
#define CAM_PIN_D0    11

#define CAM_PIN_VSYNC  6
#define CAM_PIN_HREF   7
#define CAM_PIN_PCLK  13
