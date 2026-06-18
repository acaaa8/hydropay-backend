#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h> // Wajib install library "ArduinoJson" by Benoit Blanchon di Arduino IDE

// ================= PENGATURAN JARINGAN =================
const char* ssid = "NAMA_WIFI_ANDA";          // GANTI DENGAN NAMA WIFI (SAMA DENGAN LAPTOP)
const char* password = "PASSWORD_WIFI_ANDA";  // GANTI DENGAN PASSWORD WIFI
const char* backendUrl = "http://ngrok.com:3000"; // GANTI DENGAN NGROK TUNNELLING

// ================= PENGATURAN PIN =================
#define RELAY_PIN 26
#define FLOW_SENSOR_PIN 27

// ================= VARIABEL FLOW SENSOR =================
volatile uint32_t pulseCount = 0;
float currentVolume_mL = 0.0;
// Faktor kalibrasi S401 (YF-S401) umumnya F = 98 * Q.
// Artinya 1 Liter = sekitar 5880 pulse -> 1 mL = 5.88 pulse.
// Sesuaikan angka ini (5.88) jika volume air tidak akurat (lakukan kalibrasi pakai gelas ukur)
const float calibrationFactor = 5.88; 

// ================= VARIABEL STATE & TRANSAKSI =================
enum State { WAITING, DISPENSING };
State currentState = WAITING;

String currentTxId = "";
int targetVolume_mL = 0;
unsigned long lastUpdateMillis = 0;
unsigned long lastPollMillis = 0;

// Fungsi Interrupt untuk menghitung pulse dari flow sensor
void IRAM_ATTR pulseCounter() {
  pulseCount++;
}

void setup() {
  Serial.begin(115200);

  // Set pin relay
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH); // Asumsi Active-Low (HIGH = Pompa Mati)

  // Set pin flow sensor dengan PULLUP dan daftarkan Interrupt
  pinMode(FLOW_SENSOR_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(FLOW_SENSOR_PIN), pulseCounter, FALLING);

  // Mulai koneksi WiFi
  Serial.println("\n--- Menghubungkan ke WiFi ---");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Berhasil Terhubung!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
}

void loop() {
  // Cek apakah WiFi terputus
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi terputus! Mencoba reconnect...");
    WiFi.begin(ssid, password);
    delay(2000);
    return;
  }

  // --- STATE 1: MENUNGGU PESANAN BARU ---
  if (currentState == WAITING) {
    // Polling setiap 3 detik sekali ke backend untuk mencari tugas baru
    if (millis() - lastPollMillis > 3000) {
      lastPollMillis = millis();
      checkForJob();
    }
  } 
  
  // --- STATE 2: SEDANG MENUANGKAN AIR (DISPENSING) ---
  else if (currentState == DISPENSING) {
    // 1. Hitung volume air yang sudah tertuang berdasarkan pulse
    currentVolume_mL = pulseCount / calibrationFactor;

    // 2. Laporkan progress ke backend setiap 1 detik
    if (millis() - lastUpdateMillis > 1000) {
      lastUpdateMillis = millis();
      updateProgress(currentVolume_mL, "DISPENSING");
      Serial.printf("Mengisi: %.1f mL / %d mL\n", currentVolume_mL, targetVolume_mL);
    }

    // 3. Cek apakah air sudah mencapai target volume yang dibeli
    if (currentVolume_mL >= targetVolume_mL) {
      digitalWrite(RELAY_PIN, HIGH); // MATIKAN RELAY (Pompa Mati)
      
      // Update terakhir ke backend dengan status DONE
      updateProgress(targetVolume_mL, "DONE");
      Serial.printf("\nSELESAI! Berhasil menuangkan %.1f mL\n", currentVolume_mL);
      
      // Kembali ke mode standby menunggu tugas selanjutnya
      currentState = WAITING;
    }
  }
}

// ================= FUNGSI BANTUAN =================

// Bertanya ke Backend: "Adakah transaksi yang sudah dibayar?"
void checkForJob() {
  HTTPClient http;
  String url = String(backendUrl) + "/api/dispense/next-job";
  http.begin(url);
  int httpCode = http.GET();
  
  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload);
    
    if (!error) {
      const char* txId = doc["txId"];
      
      if (txId != nullptr) {
        currentTxId = String(txId);
        targetVolume_mL = doc["volume"];
        
        Serial.println("\n>>> TUGAS BARU DITEMUKAN <<<");
        Serial.println("ID Transaksi: " + currentTxId);
        Serial.println("Target Volume: " + String(targetVolume_mL) + " mL");
        
        // Reset counter dan mulai dispensing
        pulseCount = 0;
        currentVolume_mL = 0;
        currentState = DISPENSING;
        digitalWrite(RELAY_PIN, LOW); // HIDUPKAN RELAY (Pompa Menyala)
      }
    } else {
      Serial.println("Error parsing JSON dari Backend!");
    }
  } else {
    // Tampilkan error jika gagal konek ke backend
    Serial.printf("Gagal cek pesanan! HTTP Code: %d\n", httpCode);
  }
  http.end();
}

// Lapor progress tetesan air ke Backend
void updateProgress(float filled, String status) {
  HTTPClient http;
  String url = String(backendUrl) + "/api/dispense/update";
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  
  JsonDocument doc;
  doc["txId"] = currentTxId;
  doc["filled"] = filled;
  doc["status"] = status;
  
  String requestBody;
  serializeJson(doc, requestBody);
  
  http.POST(requestBody);
  http.end();
}