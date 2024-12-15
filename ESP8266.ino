#include <LoRa.h>
#include <ESP8266WiFi.h>
#include <Adafruit_MQTT.h>
#include <Adafruit_MQTT_Client.h>
#include <WiFiClientSecure.h>

// Definición de pines LoRa
#define SS 15
#define RST 16
#define DIO0 2

//Variables recibidas paquete de LoRa
float temperatureT = 0.00;
float temperatureF = 0.00;
float tdsValue = 0.00;
float oxigeno = 0.00;
float ph_act = 0.00;
float nivelTanque = 0.00;


// Red WiFi
#define WIFI_SSID "BAML99"
#define WIFI_PASS "********"

// Datos usuario Adafruit
#define AIO_SERVER "io.adafruit.com"
#define AIO_SERVERPORT 1883 // Puerto MQTT
#define AIO_USERNAME "Tesis_02_2024"
#define AIO_KEY "****************************"

// Feeds Adafruit
#define FEED_TEMPERATURA_TANQUE "Tesis_02_2024/feeds/Temperatura-del-tanque"
#define FEED_TEMPERATURA_LAGUNA "Tesis_02_2024/feeds/Temperatura-Laguna"
#define FEED_TDS "Tesis_02_2024/feeds/TDS"
#define FEED_OXIGENO "Tesis_02_2024/feeds/Oxígeno"
#define FEED_PH "Tesis_02_2024/feeds/pH"
#define FEED_NIVEL "Tesis_02_2024/feeds/Nivel"

// Telegram Bot
#define TELEGRAM_BOT_TOKEN "**************************"


// Cliente WiFi y MQTT
WiFiClient client;
Adafruit_MQTT_Client mqtt(&client, AIO_SERVER, AIO_SERVERPORT, AIO_USERNAME, AIO_KEY);

// Instancias de los feeds MQTT
Adafruit_MQTT_Publish pubTempTanque = Adafruit_MQTT_Publish(&mqtt, FEED_TEMPERATURA_TANQUE);
Adafruit_MQTT_Publish pubTempLaguna = Adafruit_MQTT_Publish(&mqtt, FEED_TEMPERATURA_LAGUNA);
Adafruit_MQTT_Publish pubTDS = Adafruit_MQTT_Publish(&mqtt, FEED_TDS);
Adafruit_MQTT_Publish pubOxigeno = Adafruit_MQTT_Publish(&mqtt, FEED_OXIGENO);
Adafruit_MQTT_Publish pubPH = Adafruit_MQTT_Publish(&mqtt, FEED_PH);
Adafruit_MQTT_Publish pubNivel = Adafruit_MQTT_Publish(&mqtt, FEED_NIVEL);

WiFiClientSecure telegramClient;

void setup() {
  Serial.begin(9600);
  while (!Serial);

  // Configuración LoRa
  LoRa.setPins(SS, RST, DIO0);
  if (!LoRa.begin(915E6)) {
    Serial.println("Error al inicializar LoRa");
    while (1);
  }

  // Conexiones LoRa, WiFi y Adafruit
  Serial.println("LoRa inicializado correctamente");
  connectWiFi();
  connectMQTT();
  telegramClient.setInsecure();
}

void loop() {
  // Conexión MQTT
  if (!mqtt.connected()) {
    connectMQTT();
  }
  mqtt.processPackets(100);
  mqtt.ping();

  int packetSize = LoRa.parsePacket();

  if (packetSize) {
    Serial.println("Recibiendo datos...");
    String receivedData = "";

    // Leer datos del Arduino LoRa
    while (LoRa.available()) {
      receivedData += (char)LoRa.read();
    }

    Serial.println("Datos recibidos: " + receivedData);

    // Separación de paquete LoRa
    float parsedValues[6];
    int index = 0;

    while (receivedData.indexOf(",") > 0 && index < 6) {
      parsedValues[index] = receivedData.substring(0, receivedData.indexOf(",")).toFloat();
      receivedData = receivedData.substring(receivedData.indexOf(",") + 1);
      index++;
    }

    if (receivedData.length() > 0) {
      parsedValues[index] = receivedData.toFloat();
    }

    temperatureT = parsedValues[0];
    temperatureF = parsedValues[1];
    tdsValue = parsedValues[2];
    oxigeno = parsedValues[3];
    ph_act = parsedValues[4];
    nivelTanque = parsedValues[5];

    // Envío de datos a Adafruit IO
    if (pubTempTanque.publish(temperatureT)) Serial.println("Temperatura del tanque publicada");
    if (pubTempLaguna.publish(temperatureF)) Serial.println("Temperatura de la laguna publicada");
    if (pubTDS.publish(tdsValue)) Serial.println("TDS publicado");
    if (pubOxigeno.publish(oxigeno)) Serial.println("Oxígeno publicado");
    if (pubPH.publish(ph_act)) Serial.println("pH publicado");
    if (pubNivel.publish(nivelTanque)) Serial.println("Nivel del tanque publicado");

    // Alerta de Telegram si el nivel del tanque es menor o igual al 20%
    if (nivelTanque <= 20) {
      sendTelegramAlert(nivelTanque);
    }
  }
}


void connectWiFi() {
  Serial.print("Conectando a WiFi...");
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }

  Serial.println("\nConectado a WiFi");
  Serial.println("IP: " + WiFi.localIP().toString());
}


void connectMQTT() {
  while (mqtt.connect() != 0) {
    Serial.println("Error al conectar a Adafruit");
    delay(5000);
  }
  Serial.println("Conectado a Adafruit");
}


void sendTelegramAlert(float nivel) {
  String message = "⚠️⚠️⚠️ Alerta: El nivel del tanque está al " + String(nivel) + "%. ⚠️⚠️⚠️";

  // Listado de usuarios para alerta
  String chatIDs[] = {"1018001460", "7750893467", "ID"}; // ID de Telegram

  for (int i = 0; i < sizeof(chatIDs) / sizeof(chatIDs[0]); i++) {
    String url = "https://api.telegram.org/bot" + String(TELEGRAM_BOT_TOKEN) + "/sendMessage?chat_id=" + chatIDs[i] + "&text=" + message;
    Serial.println("Enviando alerta a Telegram (chat ID: " + chatIDs[i] + "): " + message);

    if (telegramClient.connect("api.telegram.org", 443)) {
      telegramClient.println("GET " + url + " HTTP/1.1");
      telegramClient.println("Host: api.telegram.org");
      telegramClient.println("Connection: close");
      telegramClient.println();
      delay(1000); // Pausa que evita saturació de conexión con el bot
    } else {
      Serial.println("Error al conectar con Telegram para chat ID: " + chatIDs[i]);
    }

    // Leer respuesta (opcional para depuración)
    while (telegramClient.available()) {
      String response = telegramClient.readString();
      Serial.println("Respuesta de Telegram para chat ID " + chatIDs[i] + ": " + response);
    }
  }
}
