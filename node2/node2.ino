#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>

// Cam bien & relay
#define DHTPIN 4
#define DHTTYPE DHT11
#define PIR_PIN 14
#define TRIG_PIN 13
#define ECHO_PIN 12
#define RELAY_PIN 27

DHT dht(DHTPIN, DHTTYPE);

// WiFi & MQTT
const char* ssid = "Iphone 12 pro max";
const char* password = "111111112";
const char* mqtt_server = "172.20.10.2";

WiFiClient espClient;
PubSubClient client(espClient);

String relayStatus = "OFF";
unsigned long lastSend = 0;
const unsigned long sendInterval = 5000; // 5s

// Do khoang cach
float readDistance() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  long duration = pulseIn(ECHO_PIN, HIGH);
  return duration * 0.034 / 2.0;
}

// Nhan MQTT
void callback(char* topic, byte* payload, unsigned int length) {
  String msg;
  for (int i = 0; i < length; i++) {
    msg += (char)payload[i];
  }

  Serial.print("📩 Lenh MQTT: ");
  Serial.println(msg);

  if (String(topic) == "control/node2/relay") {
    if (msg == "ON") {
      digitalWrite(RELAY_PIN, HIGH);
      relayStatus = "ON";
    } else if (msg == "OFF") {
      digitalWrite(RELAY_PIN, LOW);
      relayStatus = "OFF";
    }
  }
}

// Ket noi WiFi
void setup_wifi() {
  Serial.print("Dang ket noi WiFi...");
  WiFi.begin(ssid, password);
  int retry = 0;
  while (WiFi.status() != WL_CONNECTED && retry < 30) {
    delay(500);
    Serial.print(".");
    retry++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✅ Da ket noi WiFi");
    Serial.print("IP: "); Serial.println(WiFi.localIP());
  } else {
    Serial.println("\n❌ WiFi that bai!");
  }
}

// Ket noi lai MQTT
void reconnect() {
  while (!client.connected()) {
    Serial.print("🔄 Dang ket noi MQTT...");
    if (client.connect("ESP32Node2")) {
      Serial.println("✅ MQTT da ket noi");
      client.subscribe("control/node2/relay");
    } else {
      Serial.print("❌ Loi: ");
      Serial.println(client.state());
      delay(3000);
    }
  }
}

// SETUP
void setup() {
  Serial.begin(115200);
  dht.begin();

  pinMode(PIR_PIN, INPUT);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);

  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
}

// LOOP
void loop() {
  if (!client.connected()) reconnect();
  client.loop(); // Bat buoc goi moi vong lap

  unsigned long now = millis();
  if (now - lastSend >= sendInterval) {
    lastSend = now;

    float temp = dht.readTemperature();
    float hum = dht.readHumidity();
    int motion = digitalRead(PIR_PIN);
    float distance = readDistance();

    if (isnan(temp) || isnan(hum)) {
      Serial.println("❌ Loi DHT");
      return;
    }

    Serial.println("=== Node 2 ===");
    Serial.printf("Temp: %.2f C\n", temp);
    Serial.printf("Hum: %.2f %%\n", hum);
    Serial.print("Motion: "); Serial.println(motion ? "CO" : "KHONG");
    Serial.printf("Distance: %.2f cm\n", distance);
    Serial.println("Relay: " + relayStatus);

    char payload[200];
    snprintf(payload, sizeof(payload),
      "{\"motion\":%d,\"distance\":%.2f,\"temp\":%.2f,\"hum\":%.2f,\"relay\":\"%s\"}",
      motion, distance, temp, hum, relayStatus.c_str()
    );

    client.publish("sensor/node2", payload);
    Serial.println("✔️ Da gui MQTT\n");
  }
}
