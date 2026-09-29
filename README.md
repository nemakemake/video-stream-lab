# Video Stream Lab

Инструмент для анализа и ковыряния в видео потоке с ESP32-S3 с ov5640 модулем камеры.
Автоэкспозиция, автоусиление + баланс белого, настройка JPEG качества управление настройками камеры в online режиме

В планах добавить метрики канала (FPS/битрейт по времени), 2D FFT спектр, гистограмма, waveform/vectorscope

## Прошивка: настройка Wi-Fi

`firmware/src/wifi_credentials.h` не хранится в репозитории (реальный пароль сети).
Перед первой прошивкой:

```bash
cp firmware/src/wifi_credentials.h.example firmware/src/wifi_credentials.h
```

и впишите туда свои `WIFI_SSID`/`WIFI_PASSWORD`.
