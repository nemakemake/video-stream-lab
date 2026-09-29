// Video Stream Lab — прошивка ESP32-S3 + OV5640.
// Стримит JPEG по TCP и принимает JSON-команды настройки камеры по control-каналу.

#include <WiFi.h>
#include <esp_camera.h>
#include "camera_pins.h"
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

    if (psramFound()) {
        config.frame_size = FRAMESIZE_VGA;
        config.jpeg_quality = 12;
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

// [4 байта длины, big-endian][JPEG]. false — если запись не удалась.
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

// Разбор без внешних зависимостей: {"cmd":"set","param":"exposure","value":300}
// или {"cmd":"get"}.
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
    unsigned long now = millis();
    if (now - lastRssiLogMs >= 1000) {
        lastRssiLogMs = now;
        Serial.printf("[wifi] RSSI: %d dBm\n", WiFi.RSSI());
    }

    // новый клиент забирает слот всегда, иначе сервер может зависнуть после
    // неудачной записи в старого клиента
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
