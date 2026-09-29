// Video Stream Lab — прошивка ESP32-S3 + OV5640.
// Milestone 1+2: стримит JPEG по TCP (протокол v1, см. README проекта) и
// принимает JSON-команды настройки камеры по отдельному control-каналу.
// Совместима 1-в-1 с mock/mock_server.py — Qt-приложение не отличает одно от другого.

#include <WiFi.h>
#include <esp_camera.h>
#include "camera_pins.h"
// Реальные SSID/пароль — в wifi_credentials.h (не в репозитории, см. .example).
#include "wifi_credentials.h"

static const uint16_t VIDEO_PORT = 3333;
static const uint16_t CONTROL_PORT = 3334;

WiFiServer videoServer(VIDEO_PORT);
WiFiServer controlServer(CONTROL_PORT);
WiFiClient videoClient;
WiFiClient controlClient;

static bool initCamera()
{
    camera_config_t config = {};
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;
    config.pin_d0 = CAM_PIN_D0;
    config.pin_d1 = CAM_PIN_D1;
    config.pin_d2 = CAM_PIN_D2;
    config.pin_d3 = CAM_PIN_D3;
    config.pin_d4 = CAM_PIN_D4;
    config.pin_d5 = CAM_PIN_D5;
    config.pin_d6 = CAM_PIN_D6;
    config.pin_d7 = CAM_PIN_D7;
    config.pin_xclk = CAM_PIN_XCLK;
    config.pin_pclk = CAM_PIN_PCLK;
    config.pin_vsync = CAM_PIN_VSYNC;
    config.pin_href = CAM_PIN_HREF;
    config.pin_sccb_sda = CAM_PIN_SIOD;
    config.pin_sccb_scl = CAM_PIN_SIOC;
    config.pin_pwdn = CAM_PIN_PWDN;
    config.pin_reset = CAM_PIN_RESET;
    config.xclk_freq_hz = 20000000;
    config.pixel_format = PIXFORMAT_JPEG;

    // С PSRAM можно себе позволить кадр покрупнее и двойную буферизацию
    // (второй буфер снижает шанс порванного кадра при медленном клиенте).
    if (psramFound()) {
        config.frame_size = FRAMESIZE_VGA; // 640x480 — компромисс для старта
        config.jpeg_quality = 12;          // 0..63, меньше = лучше качество/больше размер
        config.fb_count = 2;
        config.fb_location = CAMERA_FB_IN_PSRAM;
        config.grab_mode = CAMERA_GRAB_LATEST;
    } else {
        config.frame_size = FRAMESIZE_QVGA;
        config.jpeg_quality = 15;
        config.fb_count = 1;
        config.fb_location = CAMERA_FB_IN_DRAM;
        config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
    }

    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        Serial.printf("Camera init failed: 0x%x\n", err);
        return false;
    }
    return true;
}

// Отправляет один JPEG-кадр в видео-канал по протоколу v1:
// [4 байта длины, big-endian][JPEG-байты].
// Возвращает false, если запись не удалась (клиент отвалился) — тогда
// вызывающий код обязан закрыть videoClient, чтобы освободить слот для
// нового подключения (см. комментарий в loop()).
static bool sendFrame(WiFiClient &client, camera_fb_t *fb)
{
    uint8_t header[4] = {
        static_cast<uint8_t>((fb->len >> 24) & 0xFF),
        static_cast<uint8_t>((fb->len >> 16) & 0xFF),
        static_cast<uint8_t>((fb->len >> 8) & 0xFF),
        static_cast<uint8_t>(fb->len & 0xFF),
    };
    size_t written = client.write(header, sizeof(header));
    written += client.write(fb->buf, fb->len);
    return written == sizeof(header) + fb->len;
}

// Очень простой разбор ожидаемого формата команд без внешних зависимостей:
// {"cmd":"set","param":"exposure","value":300}
// Для более сложных команд в будущем стоит перейти на ArduinoJson.
// Отвечает на {"cmd":"get"} текущими настройками сенсора (те же имена, что у set):
// {"settings":{"auto_exposure":1,"exposure":300,"auto_gain":1,"gain":0,"whitebal":1,"jpeg_quality":12}}
static void sendSettings()
{
    sensor_t *sensor = esp_camera_sensor_get();
    if (!sensor || !controlClient || !controlClient.connected())
        return;

    const camera_status_t &s = sensor->status;
    controlClient.printf(
        "{\"settings\":{\"auto_exposure\":%d,\"exposure\":%d,\"auto_gain\":%d,\"gain\":%d,"
        "\"whitebal\":%d,\"jpeg_quality\":%d}}\n",
        s.aec, s.aec_value, s.agc, s.agc_gain, s.awb, s.quality);
}

static void handleControlLine(const String &line)
{
    if (line.indexOf("\"get\"") >= 0 && line.indexOf("\"param\"") < 0) {
        sendSettings();
        return;
    }

    int paramIdx = line.indexOf("\"param\"");
    int valueIdx = line.indexOf("\"value\"");
    if (paramIdx < 0 || valueIdx < 0)
        return;

    int paramStart = line.indexOf('"', line.indexOf(':', paramIdx) + 1) + 1;
    int paramEnd = line.indexOf('"', paramStart);
    String param = line.substring(paramStart, paramEnd);

    int valueColon = line.indexOf(':', valueIdx);

    // Значение может быть числом без кавычек — читаем до запятой/скобки.
    int vStart = valueColon + 1;
    while (vStart < (int)line.length() && (line[vStart] == ' '))
        vStart++;
    int vEnd = vStart;
    while (vEnd < (int)line.length() && line[vEnd] != ',' && line[vEnd] != '}')
        vEnd++;
    long value = line.substring(vStart, vEnd).toInt();

    sensor_t *sensor = esp_camera_sensor_get();
    if (!sensor)
        return;

    if (param == "exposure") {
        sensor->set_exposure_ctrl(sensor, 0);
        sensor->set_aec_value(sensor, value);
    } else if (param == "gain") {
        sensor->set_gain_ctrl(sensor, 0);
        sensor->set_agc_gain(sensor, value);
    } else if (param == "auto_exposure") {
        sensor->set_exposure_ctrl(sensor, value ? 1 : 0);
    } else if (param == "auto_gain") {
        sensor->set_gain_ctrl(sensor, value ? 1 : 0);
    } else if (param == "whitebal") {
        sensor->set_whitebal(sensor, value ? 1 : 0);
    } else if (param == "jpeg_quality") {
        sensor->set_quality(sensor, value);
    }

    Serial.printf("[control] %s = %ld\n", param.c_str(), value);

    if (controlClient && controlClient.connected()) {
        controlClient.printf("{\"ack\":true,\"param\":\"%s\",\"value\":%ld}\n", param.c_str(), value);
    }
}

void setup()
{
    Serial.begin(115200);
    delay(300);

    if (!initCamera()) {
        Serial.println("Останов: камера не инициализирована.");
        return;
    }

    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.print("Подключение к Wi-Fi");
    while (WiFi.status() != WL_CONNECTED) {
        delay(300);
        Serial.print(".");
    }
    Serial.printf("\nПодключено. IP: %s\n", WiFi.localIP().toString().c_str());

    videoServer.begin();
    controlServer.begin();
    Serial.printf("Видео-сервер: порт %u, control-сервер: порт %u\n", VIDEO_PORT, CONTROL_PORT);
}

static unsigned long lastRssiLogMs = 0;

void loop()
{
    // Диагностика: раз в секунду печатаем уровень сигнала Wi-Fi, чтобы отличить
    // "плохой сигнал/просадка питания" от других причин обрыва связи.
    unsigned long now = millis();
    if (now - lastRssiLogMs >= 1000) {
        lastRssiLogMs = now;
        Serial.printf("[wifi] RSSI: %d dBm\n", WiFi.RSSI());
    }

    // Переподключение видео-клиента (принимаем только одного за раз — этого
    // достаточно для Milestone 1, простой sequential accept).
    //
    // Важно: новый клиент забирает слот, даже если videoClient.connected()
    // всё ещё зачем-то говорит true — после неудачной записи (см. ниже)
    // WiFiClient не всегда сразу отражает разрыв, и без explicit stop()
    // сервер мог навечно "зависнуть", отказывая всем новым подключениям.
    if (videoServer.hasClient()) {
        if (videoClient) {
            videoClient.stop();
        }
        videoClient = videoServer.available();
        Serial.println("[video] клиент подключился");
    }

    if (videoClient && videoClient.connected()) {
        camera_fb_t *fb = esp_camera_fb_get();
        if (fb) {
            if (!sendFrame(videoClient, fb)) {
                Serial.println("[video] запись не удалась — закрываю клиента");
                videoClient.stop();
            }
            esp_camera_fb_return(fb);
        }
    }

    // Control-канал: неблокирующее чтение строк.
    if (!controlClient || !controlClient.connected()) {
        WiFiClient newClient = controlServer.available();
        if (newClient) {
            controlClient = newClient;
            Serial.println("[control] клиент подключился");
        }
    }
    if (controlClient && controlClient.connected() && controlClient.available()) {
        String line = controlClient.readStringUntil('\n');
        if (line.length() > 0)
            handleControlLine(line);
    }
}
