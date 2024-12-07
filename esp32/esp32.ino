#include <PubSubClient.h>

#include "esp_camera.h"
#include <WiFi.h>
#include <WiFiUdp.h>
#include <HTTPClient.h>
#include <WiFiClient.h>
// #include <esp32-camera.h>

const char* ssid = "POCO X3 Pro";   // Ganti dengan SSID jaringan Anda
const char* password = "12345687";  // Ganti dengan password jaringan Anda

#define FLASH_PIN 4  // Pin yang terhubung ke LED flash
bool photoTaken = false;

WiFiServer server(5000);  // Server HTTP berjalan di port 80
String ip = "192.168.1.6";
String serverName = "http://" + ip;
String serverPort = "5000";
String targetStream = serverName + serverPort + "/upload";

// Connect to server using IP address
IPAddress serverIP(192, 168, 1, 6);


camera_config_t config;
sensor_t* s;

unsigned long previousMillis = 0;
const long interval = 2 * 60 * 60 * 1000;  // 2 jam dalam milidetik
bool firstRun = true;                      // Flag untuk menandakan pertama kali dijalankan

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);

  // Tunggu koneksi WiFi
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("Connected to WiFi");



  pinMode(FLASH_PIN, OUTPUT);
  digitalWrite(FLASH_PIN, LOW);  // Pastikan flash mati awalnya

  // Konfigurasi kamera
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = 5;         // Data pin 0
  config.pin_d1 = 18;        // Data pin 1
  config.pin_d2 = 19;        // Data pin 2
  config.pin_d3 = 21;        // Data pin 3
  config.pin_d4 = 36;        // Data pin 4
  config.pin_d5 = 39;        // Data pin 5
  config.pin_d6 = 34;        // Data pin 6
  config.pin_d7 = 35;        // Data pin 7
  config.pin_xclk = 0;       // XCLK pin
  config.pin_pclk = 22;      // PCLK pin
  config.pin_vsync = 25;     // VSYNC pin
  config.pin_href = 23;      // HREF pin
  config.pin_sscb_sda = 26;  // SDA pin untuk I2C
  config.pin_sscb_scl = 27;  // SCL pin untuk I2C
  config.pin_pwdn = 32;      // Power down pin, set ke -1 jika tidak digunakan
  config.pin_reset = -1;     // Reset pin, set ke -1 jika tidak digunakan
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;  // Untuk streaming

  // Inisialisasi kamera
  config.frame_size = FRAMESIZE_VGA;  // Ukuran frame
  config.jpeg_quality = 12;           // Kualitas JPEG
  config.fb_count = 1;                // Untuk modul AI Thinker

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.println("Camera initialization failed!");
    return;
  }

  // Terapkan pengaturan kamera
  s = esp_camera_sensor_get();
  s->set_quality(s, 20);     // Set kualitas gambar
  s->set_saturation(s, 0);   // Atur saturasi warna
  s->set_sharpness(s, 0);    // Atur ketajaman
  s->set_brightness(s, +2);  // Atur kecerahan
  s->set_contrast(s, -1);    // Atur kontras
  s->set_whitebal(s, 1);     // Aktifkan keseimbangan putih
  s->set_hmirror(s, 0);      // Cermin horizontal
  s->set_vflip(s, 0);        // Cermin vertikal

  // Mulai server
  server.begin();
  Serial.print("Server started at: ");
  Serial.println(WiFi.localIP());

  previousMillis = millis();
}

void loop() {
  unsigned long currentMillis = millis();

  // Proses streaming atau fungsi lainnya
  camera_fb_t* fb = esp_camera_fb_get();
  WiFiClient client = server.available();

  // Cek apakah sudah waktunya mengirim foto
  // Kondisi berbeda untuk pertama kali dijalankan
  if (firstRun || (currentMillis - previousMillis >= interval)) {
    // Jika ini pertama kali atau sudah melewati interval
    previousMillis = currentMillis;

    // Reset flag firstRun setelah pertama kali dijalankan
    firstRun = false;

    // Kirim foto ke server
    sendToServer( client);
  }

  if (!fb) {
    Serial.println("Camera capture failed");
    return;
  }

  // Kirim frame ke server
  if (sendFrameToServer(fb, client)) {
    Serial.println("Frame sent successfully");
  } else {
    Serial.println("Failed to send frame");
  }

  esp_camera_fb_return(fb);
  delay(1000);

  client.stop();
}


bool sendFrameToServer(camera_fb_t* fb, WiFiClient& client) {
  if (!fb || fb->len == 0) {
    Serial.println("Invalid frame buffer");
    return false;
  }

  int connectionAttempts = 0;
  while (connectionAttempts < 3) {
    WiFiClient newClient;

    if (newClient.connect(ip.c_str(), 5000)) {
      Serial.println("Connected to server");

      // Kirim langsung raw image data
      newClient.println("POST /upload HTTP/1.1");
      newClient.println("Host: " + ip);
      newClient.println("Content-Type: image/jpeg");
      newClient.println("Content-Length: " + String(fb->len));
      newClient.println("Connection: close");
      newClient.println();

      // Kirim buffer gambar
      size_t bytesSent = newClient.write(fb->buf, fb->len);

      Serial.print("Bytes sent: ");
      Serial.println(bytesSent);

      // Tunggu respons
      unsigned long startTime = millis();
      while (newClient.connected() && (millis() - startTime < 5000)) {
        if (newClient.available()) {
          String response = newClient.readStringUntil('\n');
          Serial.println("Server Response: " + response);

          if (response.startsWith("HTTP/1.1 200") || response.startsWith("HTTP/1.1 204")) {
            newClient.stop();
            return true;
          }
        }
      }

      newClient.stop();
      Serial.println("Failed to get valid server response");
    } else {
      Serial.println("Connection failed");
    }

    connectionAttempts++;
    delay(250);
  }

  return false;
}

void sendToServer(WiFiClient& client) {
  digitalWrite(FLASH_PIN, HIGH);  // Nyalakan flash
  delay(100);                     // Tunggu sebentar agar flash menyala

  camera_fb_t* fb = NULL;
  int captureAttempts = 0;
  const int maxAttempts = 5;

  // Coba beberapa kali untuk mengambil foto
  while (captureAttempts < maxAttempts) {
    // Reset sensor kamera sebelum capture
    esp_camera_deinit();
    
    // Inisialisasi ulang konfigurasi kamera
    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
      Serial.println("Camera re-initialization failed!");
      captureAttempts++;
      delay(500);
      continue;
    }

    // Atur ulang pengaturan sensor
    sensor_t* s = esp_camera_sensor_get();
    if (s) {
      // Reset dan atur ulang parameter kamera
      s->set_quality(s, 10);
      s->set_brightness(s, 0);
      s->set_contrast(s, 0);
      s->set_saturation(s, 0);
      s->set_sharpness(s, 1);
    }

    // Tunggu sebentar sebelum capture
    delay(200);

    // Coba ambil foto
    fb = esp_camera_fb_get();
    
    if (fb) {
      // Periksa ukuran foto
      if (fb->len > 1024) {  // Pastikan foto memiliki ukuran minimal
        break;  // Foto berhasil
      } else {
        // Foto terlalu kecil, kembalikan buffer
        esp_camera_fb_return(fb);
        fb = NULL;
      }
    }

    Serial.print("Capture attempt failed: ");
    Serial.println(captureAttempts);
    
    captureAttempts++;
    delay(500);  // Tunggu sebentar sebelum percobaan berikutnya
  }

  // Cek hasil akhir
  if (!fb) {
    Serial.println("Failed to capture image after multiple attempts");
    digitalWrite(FLASH_PIN, LOW);
    return;
  }

  // Proses pengiriman foto
  String target = serverName + "/ekstrak";
  if (!client.connect(ip.c_str(), 5000)) {
    Serial.println("Connection to server failed");
    esp_camera_fb_return(fb);
    digitalWrite(FLASH_PIN, LOW);
    return;
  }

  // Lanjutkan dengan proses pengiriman seperti sebelumnya
  String head = "--boundary\r\nContent-Disposition: form-data; name=\"file\"; filename=\"esp32-cam.jpg\"\r\nContent-Type: image/jpeg\r\n\r\n";
  String tail = "\r\n--boundary--\r\n";
  uint32_t totalLen = head.length() + fb->len + tail.length();

  client.println("POST " + target + " HTTP/1.1");
  client.println("Host: " + ip);
  client.println("Content-Length: " + String(totalLen));
  client.println("Content-Type: multipart/form-data; boundary=boundary");
  client.println();

  client.print(head);
  client.write(fb->buf, fb->len);
  client.print(tail);

  // Tunggu respons server
  unsigned long startTime = millis();
  while (client.connected() || client.available()) {
    if (client.available()) {
      String line = client.readStringUntil('\n');
      Serial.println(line);
    }
    
    // Tambahkan timeout
    if (millis() - startTime > 10000) {  // 10 detik timeout
      Serial.println("Server response timeout");
      break;
    }
  }

  // Bersihkan
  esp_camera_fb_return(fb);
  client.stop();
  
  Serial.println("Photo sent successfully");
  digitalWrite(FLASH_PIN, LOW);  // Matikan flash

  delay(100);
}