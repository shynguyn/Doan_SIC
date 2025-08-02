#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>

// ==== Cảm biến & chân kết nối ====
#define DHTPIN 4
#define DHTTYPE DHT11
#define MQ135_PIN 34
#define RELAY_PIN 19
#define GAS_THRESHOLD 300   // Ngưỡng khí gas

DHT dht(DHTPIN, DHTTYPE);

// ==== WiFi & MQTT cấu hình ====
const char* ssid = "Iphone 12 pro max";
const char* password = "111111112";
const char* mqtt_server = "172.20.10.2";  // MQTT Broker

WiFiClient espClient;
PubSubClient client(espClient);

// ==== Biến trạng thái ====
String relayState = "OFF";
bool autoControl = true;

// ==== Kết nối WiFi ====
void setup_wifi() {
  Serial.println("🔌 Kết nối WiFi...");
  WiFi.begin(ssid, password);

  int retry = 0;
  while (WiFi.status() != WL_CONNECTED && retry++ < 30) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✅ Đã kết nối WiFi!");
    Serial.print("IP: "); Serial.println(WiFi.localIP());
  } else {
    Serial.println("\n❌ Kết nối WiFi thất bại.");
  }
}

// ==== Cập nhật trạng thái relay ====
void setRelay(bool on) {
  digitalWrite(RELAY_PIN, on ? HIGH : LOW);
  relayState = on ? "ON" : "OFF";
}

// ==== Xử lý lệnh MQTT ====
void callback(char* topic, byte* message, unsigned int length) {
  String msg;
  for (unsigned int i = 0; i < length; i++) msg += (char)message[i];

  Serial.print("📩 Nhận lệnh: "); Serial.println(msg);

  if (String(topic) == "control/node1/relay") {
    if (msg == "ON") {
      setRelay(true);
      autoControl = false;
    } else if (msg == "OFF") {
      setRelay(false);
      autoControl = false;
    } else if (msg == "AUTO") {
      autoControl = true;
      Serial.println("🔁 Chế độ tự động relay đã bật.");
    }
  }
}

// ==== Kết nối lại MQTT ====
void reconnect_mqtt() {
  while (!client.connected()) {
    Serial.print("🔄 Kết nối lại MQTT...");
    if (client.connect("ESP32_Node1")) {
      Serial.println(" ✅ Thành công!");
      client.subscribe("control/node1/relay");
    } else {
      Serial.print(" ❌ lỗi: "); Serial.println(client.state());
      delay(3000);
    }
  }
}

// ==== Gửi dữ liệu cảm biến lên MQTT ====
void publishSensorData(float temp, float hum, int gas) {
  char payload[200];
  snprintf(payload, sizeof(payload),
           "{\"temp\":%.2f,\"hum\":%.2f,\"gas\":%d,\"relay\":\"%s\"}",
           temp, hum, gas, relayState.c_str());

  if (client.publish("sensor/node1", payload)) {
    Serial.println("✅ Gửi MQTT thành công\n");
  } else {
    Serial.println("❌ Gửi MQTT thất bại\n");
  }
}

// ==== Khởi động ====
void setup() {
  Serial.begin(115200);
  dht.begin();
  pinMode(RELAY_PIN, OUTPUT);
  setRelay(false);  // Relay OFF lúc khởi động

  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
}

// ==== Vòng lặp chính ====
void loop() {
  if (!client.connected()) reconnect_mqtt();
  client.loop();

  float temp = dht.readTemperature();
  float hum = dht.readHumidity();
  int gas = analogRead(MQ135_PIN);

  if (isnan(temp) || isnan(hum)) {
    Serial.println("❌ Lỗi đọc DHT11!");
    delay(2000);
    return;
  }

  Serial.printf("🌡️ Nhiệt độ: %.2f °C | 💧 Độ ẩm: %.2f %% | 🧪 Gas: %d\n", temp, hum, gas);

  // ==== Điều khiển relay tự động ====
  if (autoControl) {
    if (gas > GAS_THRESHOLD && relayState != "ON") {
      setRelay(true);
    } else if (gas <= GAS_THRESHOLD && relayState != "OFF") {
      setRelay(false);
    }
  }

  publishSensorData(temp, hum, gas);
  delay(5000);
}
