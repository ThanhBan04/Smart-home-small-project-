// ================= THƯ VIỆN =================
#include <Wire.h>                
#include <LiquidCrystal_I2C.h>   
#include <DHT22.h>               
#include <Ticker.h>              

// ================= BLYNK =================
#define BLYNK_TEMPLATE_ID "TMPL6d4HS4UJr"
#define BLYNK_TEMPLATE_NAME "Smart home"
#define BLYNK_AUTH_TOKEN "SckreLJg2yKloMNGBmOalxiCjU-1_hiO"

#include <WiFi.h>
#include <BlynkSimpleEsp32.h>

// ================= WIFI =================
char ssid[] = "IPhone XR"; 
char pass[] = "12345689101112";

// ================= KHAI BÁO CHÂN =================
#define DHTPIN       9    
#define LDR_PIN      1    
#define RELAY_PIN    2    
#define BUTTON_PIN   6    
#define BUZZER_PIN   7    
#define GAS_PIN      3    

// ================= NGƯỠNG =================
const int LDR_THRESHOLD = 2000;   
const int GAS_THRESHOLD = 2000;   

// ================= ĐỐI TƯỢNG =================
DHT22 dht(DHTPIN);
LiquidCrystal_I2C lcd(0x27, 16, 2);

Ticker dhtTimer;
Ticker ldrTimer;
Ticker gasTimer;
BlynkTimer blynkTimer;

// ================= BIẾN TOÀN CỤC =================
volatile bool buttonFlag = false;
volatile unsigned long lastInterruptTime = 0;

float lastTemp = 0;
float lastHum = 0;
int lastLdr = 0;
int lastGas = 0;

bool gasAlertActive = false;
bool buzzerManual = false;
bool manualGasMute = false;

volatile bool readDHTFlag = false;
volatile bool readLDRFlag = false;
volatile bool readGasFlag = false;

// ================= ISR =================
void IRAM_ATTR handleButtonPress() {
  unsigned long now = millis();
  if (now - lastInterruptTime > 1000) {
    buttonFlag = true;
    lastInterruptTime = now;
  }
}
void setReadDHTFlag() { readDHTFlag = true; }
void setReadLDRFlag() { readLDRFlag = true; }
void setReadGasFlag() { readGasFlag = true; }

// ================= BLYNK SWITCH (V0) =================
BLYNK_WRITE(V0) {
  int switchState = param.asInt();
  if (switchState == 1) {
    buzzerManual = true;
    digitalWrite(BUZZER_PIN, HIGH);
    Serial.println("[BLYNK] Còi bật thủ công qua app");
    Blynk.virtualWrite(V1, "Manual Warning");
  } else {
    buzzerManual = false;
    digitalWrite(BUZZER_PIN, LOW);
    Serial.println("[BLYNK] Còi tắt thủ công qua app");
    Blynk.virtualWrite(V1, "Safe");
  }
}

// ================= GỬI DỮ LIỆU LÊN BLYNK =================
void sendToBlynk() {
  Blynk.virtualWrite(V2, lastGas);  // Gauge gas
  if (gasAlertActive) {
    Blynk.virtualWrite(V1, "Gas WARNING !!!");
  } else if (buzzerManual) {
    Blynk.virtualWrite(V1, "Manual Warning");
  } else {
    Blynk.virtualWrite(V1, "Safe");
  }
}

// ================= ĐỒNG BỘ KHI KẾT NỐI LẠI =================
BLYNK_CONNECTED() {
  Blynk.syncVirtual(V0);
  Blynk.virtualWrite(V2, lastGas);
  Serial.println("[BLYNK] Đồng bộ dữ liệu khi kết nối lại");
}

// ================= SETUP =================
void setup() {
  Serial.begin(115200);
  Wire.begin(4, 5);

  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Smart Sensor Sys");

  pinMode(RELAY_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);

  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), handleButtonPress, FALLING);

  dhtTimer.attach(20, setReadDHTFlag);
  ldrTimer.attach(15, setReadLDRFlag);
  gasTimer.attach(2, setReadGasFlag);
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
  blynkTimer.setInterval(2000L, sendToBlynk);
  Serial.println("=== Hệ thống khởi động xong ===");
}

// ================= LOOP =================
void loop() {
  Blynk.run();
  blynkTimer.run();

  // --- Nút nhấn ---
  if (buttonFlag) {
    buttonFlag = false;

    if (gasAlertActive) {
      gasAlertActive = false;
      manualGasMute = true;
      digitalWrite(BUZZER_PIN, LOW);
      Serial.println("[BTN] Còi tắt do nhấn nút (Gas Alert)");
      Blynk.virtualWrite(V1, "Safe (Manual Clear)");
    } else {
      buzzerManual = !buzzerManual;
      digitalWrite(BUZZER_PIN, buzzerManual ? HIGH : LOW);
      Blynk.virtualWrite(V0, buzzerManual ? 1 : 0);
      Blynk.virtualWrite(V1, buzzerManual ? "Manual Warning" : " Safe");
      Serial.println(buzzerManual ? "[BTN] Còi bật thủ công" : "[BTN] Còi tắt thủ công");
    }
  }

  // --- DHT22 ---
  if (readDHTFlag) {
    readDHTFlag = false;
    lastHum = dht.getHumidity();
    lastTemp = dht.getTemperature();
    if (isnan(lastHum) || isnan(lastTemp)) {
      Serial.println("[DHT] Lỗi đọc sensor!");
    } else {
      Serial.printf("[DHT] T: %.1fC, H: %.0f%%\n", lastTemp, lastHum);
    }
  }

  // --- LDR ---
  if (readLDRFlag) {
    readLDRFlag = false;
    lastLdr = analogRead(LDR_PIN);
    Serial.printf("[LDR] Value = %d\n", lastLdr);
    digitalWrite(RELAY_PIN, (lastLdr > LDR_THRESHOLD) ? HIGH : LOW);
  }

  // --- MQ2 GAS ---
  if (readGasFlag) {
    readGasFlag = false;
    lastGas = analogRead(GAS_PIN);
    Serial.printf("[GAS] Value = %d\n", lastGas);
  
    if (lastGas > GAS_THRESHOLD) {
      if (!gasAlertActive && !manualGasMute) {
        gasAlertActive = true;
        digitalWrite(BUZZER_PIN, HIGH);
        Blynk.virtualWrite(V1, " Gas WARNING !!!");
        Serial.println("[GAS] !!! GAS ALERT - CÒI BẬT !!!");
      }
    } else {
      if (manualGasMute) {
        manualGasMute = false;
        Serial.println("[GAS] Gas safe again - reset manual mute");
      }
      if (!gasAlertActive && !buzzerManual) {
        digitalWrite(BUZZER_PIN, LOW);
        Blynk.virtualWrite(V1, " Safe");
      }
    }
  }

  // --- LCD ---
  lcd.setCursor(0, 0);
  if (gasAlertActive) lcd.print("Gas WARNING!!! ");
  else if (buzzerManual) lcd.print("Manual WARN!   ");
  else lcd.print("Gas Safe       ");

  lcd.setCursor(0, 1);
  if (isnan(lastTemp) || isnan(lastHum)) lcd.print("Sensor Error   ");
  else {
    lcd.print("T:");
    lcd.print(lastTemp, 1);
    lcd.print(" H:");
    lcd.print(lastHum, 0);
    lcd.print("% ");
  }
  delay(100);
}
