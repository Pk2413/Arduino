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

// //setup streaming
// WiFiUDP udp;
// const char* udpAddress = "192.168.137.1";  // Ganti dengan alamat IP tujuan
// const int udpPort = 1234;                  // Port UDP


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
config.pin_d0 = 5;      // Data pin 0
config.pin_d1 = 18;     // Data pin 1
config.pin_d2 = 19;     // Data pin 2
config.pin_d3 = 21;     // Data pin 3
config.pin_d4 = 36;     // Data pin 4
config.pin_d5 = 39;     // Data pin 5
config.pin_d6 = 34;     // Data pin 6
config.pin_d7 = 35;     // Data pin 7
config.pin_xclk = 0;    // XCLK pin
config.pin_pclk = 22;    // PCLK pin
config.pin_vsync = 25;   // VSYNC pin
config.pin_href = 23;    // HREF pin
config.pin_sscb_sda = 26; // SDA pin untuk I2C
config.pin_sscb_scl = 27; // SCL pin untuk I2C
config.pin_pwdn = 32;    // Power down pin, set ke -1 jika tidak digunakan
config.pin_reset = -1;   // Reset pin, set ke -1 jika tidak digunakan
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
  s->set_quality(s, 20);    // Set kualitas gambar
  s->set_saturation(s, 0);  // Atur saturasi warna
  s->set_sharpness(s, 0);   // Atur ketajaman
  s->set_brightness(s, 0);  // Atur kecerahan
  s->set_contrast(s, 0);    // Atur kontras
  s->set_whitebal(s, 1);    // Aktifkan keseimbangan putih
  s->set_hmirror(s, 0);     // Cermin horizontal
  s->set_vflip(s, 0);       // Cermin vertikal

  // Mulai server
  server.begin();
  Serial.print("Server started at: ");
  Serial.println(WiFi.localIP());

  //mulai streaming
  // udp.begin(udpPort);
}

void loop() {
  WiFiClient client = server.available();
  
  camera_fb_t* fb = esp_camera_fb_get();  // Ambil foto
  // if (!client) {
  //   return;
  // sendToServer(client);
  // delay(5000);
  // }

  // // Tunggu permintaan klien
  // while (!client.available()) {
  //   delay(1);
  // }

  // String request = client.readStringUntil('\r');
  // client.flush();

  // if (request.indexOf("/foto") != -1) {
  //   takePhoto(client);
  // } else if (request.indexOf("/video") != -1) {
  
  if (!fb) {
    Serial.println("Camera capture failed");
    return;
    delay(1000);
  }

  // Send the frame to the Flask server
  if (sendFrameToServer(fb, client)) {
    Serial.println("Frame sent successfully");
  } else {
    Serial.println("Failed to send frame");
  }

  
  esp_camera_fb_return(fb);
  delay(100);
  // }


  client.stop();
  Serial.println("Client disconnected");
}

void takePhoto(WiFiClient& client) {
  digitalWrite(FLASH_PIN, HIGH);  // Nyalakan flash
  delay(800);

  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Camera capture failed");
    client.println("HTTP/1.1 500 Internal Server Error\r\n\r\nCamera capture failed");
    digitalWrite(FLASH_PIN, LOW);  // Matikan flash
    return;
  } else {

    // Kirim header respons HTTP
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: image/jpeg");
    client.println("Content-Length: " + String(fb->len));
    client.println();
    client.write(fb->buf, fb->len);  // Kirim data gambar ke klien

    esp_camera_fb_return(fb);      // Kembalikan buffer frame ke pool
    digitalWrite(FLASH_PIN, LOW);  // Matikan flash
  }
}

bool sendFrameToServer(camera_fb_t* fb, WiFiClient& client) {

  if (!client.connect(ip.c_str(), 5000)) {
    Serial.println("Connection to server failed");
    return false;
  }
  String boundary = "---------------------------14737809831466499882746641449";
  String body = "--" + boundary + "\r\n";
  body += "Content-Disposition: form-data; name=\"file\"; filename=\"image.jpg\"\r\n";
  body += "Content-Type: image/jpeg\r\n\r\n";

  client.println("POST /upload HTTP/1.1");
  client.println("Host: " + ip );
  client.println("Content-Type: multipart/form-data; boundary=" + boundary);
  client.println("Content-Length: " + String(body.length() + fb->len + 4 + boundary.length() + 6));
  client.println();  // This will add an extra newline
  client.print(body);
  client.write(fb->buf, fb->len);
  client.println("\r\n--" + boundary + "--");

  // Wait for server response
  while (client.connected() || client.available()) {
    if (client.available()) {
      String line = client.readStringUntil('\n');
      Serial.println(line);
    }
  }

  client.stop();
  return true;
}


void sendToServer(WiFiClient& client) {
  if (photoTaken) {
    Serial.println("Photo already taken, skipping...");
    // Jika foto sudah diambil, keluar dari fungsi
    delay(2000);
    photoTaken = false;
    return;
  }

  digitalWrite(FLASH_PIN, HIGH);  // Nyalakan flash
  delay(100);                     // Tunggu sebentar agar flash menyala

  camera_fb_t* fb = esp_camera_fb_get();  // Ambil foto
  if (!fb) {
    Serial.println("Failed to capture image for upload");
    digitalWrite(FLASH_PIN, LOW);  // Matikan flash jika gagal
    return;
  }

  String target = serverName + "/ekstrak";
  if (!client.connect(ip.c_str(), 5000)) {  // Ganti dengan alamat server Anda
    Serial.println("Connection to server failed");
    esp_camera_fb_return(fb);
    digitalWrite(FLASH_PIN, LOW);  // Matikan flash jika gagal
    return;
  }

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
  while (client.connected() || client.available()) {
    if (client.available()) {
      String line = client.readStringUntil('\n');
      Serial.println(line);
    }
  }

  esp_camera_fb_return(fb);
  client.stop();
  Serial.println("Photo sent successfully");

  digitalWrite(FLASH_PIN, LOW);  // Matikan flash
  photoTaken = true;             // Tandai bahwa foto sudah diambil

  delay(5000);
}
