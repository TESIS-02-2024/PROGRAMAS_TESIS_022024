#include <Wire.h>
#include <SPI.h>
#include <LoRa.h>

const int lm35PinT = A0;      // Pin analógico conectado al sensor LM35 TANQUE
int lm35PinF = A1;      // Pin analógico conectado al sensor LM35 LAGUNA
const int analogPin = A2;     // Pin analógico conectado al sensor TDS
const int analogPinOD = A3;   // Pin analógico conectado al sensor OD
int s_ph = A4;                // Pin analógico conectado al sensor pH

const int nivel5 = A8;       // nivel de agua  5
const int nivel20 = A9;       // nivel de agua  20
const int nivel40 = A10;       // nivel de agua  40
const int nivel60 = A11;      // nivel de agua  60
const int nivel80 = A12;      // nivel de agua  80
const int nivel100 = A13;     // nivel de agua  100

//Frecuencia de funcionamiento LoRa
const long frequency = 915E6;


float temperatureT;   //
float temperatureF;   //
float tdsValue;
float ph_act;
int nivelTanque=0;
float oxigeno;


// Variables para sensor pH
float calibration_value = 21.34;
int phval = 0;
unsigned long int avgval;
int buffer_arr[10], temp;

// Variables para nivel de agua y sus umbrales
int lectura5,lectura20, lectura40, lectura60, lectura80, lectura100;
const int umbral5 = 100; 
const int umbral20 = 100; 
const int umbral40 = 100; 
const int umbral60 = 100; 
const int umbral80 = 100; 
const int umbral100 = 100;


//Bombas de aire
const int bombaAire1Pin = 35; // bomba de aire 1
const int bombaAire2Pin = 37; // bomba de aire 2

//Bomba peristaltica
const int bombaPeristalticaPin = 30;                //bomba peristáltica
const float tiempoPorMl = 0.73;                     // Tiempo en segundos para suministrar 1 mL
const float dosisMl = 2.6;                          // Cantidad de líquido en mL por dosis
const unsigned long intervaloDosis = 8 * 60 * 1000; // Tiempo entre dosis en milisegundos
unsigned long ultimoTiempoDosis = 0;                // Último momento en que se activó la bomba peristáltica


void setup() {
  Serial.begin(9600); 
  
  pinMode(analogPinOD, INPUT);   // Pin analógico conectado al sensor OD
  pinMode(s_ph, INPUT); // Pin del sensor de PH

  // Configuración de pines como salida
  pinMode(bombaAire1Pin, OUTPUT);
  pinMode(bombaAire2Pin, OUTPUT);
  pinMode(bombaPeristalticaPin, OUTPUT);


  // Inicialización de bombas apagadas
  digitalWrite(bombaAire1Pin, LOW);
  digitalWrite(bombaAire2Pin, LOW);
  digitalWrite(bombaPeristalticaPin, LOW);

  // Conexión del LoRa
  LoRa.setPins(10, 9, 2); // NSS, RST, DIO0
  if (!LoRa.begin(frequency)) {
    Serial.println("No se pudo conectar LoRa");
    while (1);
  }
  Serial.println("LoRa iniciado");
}


void loop() {
  medirTemperatura();
  medirTDS();
  medirNivelAgua();
  PH();
  OD();
  LORA();
  delay(2000); //
}


void medirTemperatura() {
  int sensorValueT = analogRead(lm35PinT);
  int sensorValueF = analogRead(lm35PinF);
  
  float voltageT = sensorValueT * (5.0 / 1023.0);
  float voltageF = sensorValueF * (5.0 / 1023.0);
   temperatureT = voltageT * 100.0;
   temperatureF = voltageF * 100.0;

  Serial.print("Temperatura T: ");
  Serial.print(temperatureT);
  Serial.println(" °C");
  Serial.print("Temperatura F: ");
  Serial.print(temperatureF);
  Serial.println(" °C");
}


void medirTDS() {
  int sensorValue = analogRead(analogPin);
  float voltage = sensorValue * (5.0 / 1023.0);
   tdsValue = (133.42 * voltage * voltage * voltage - 255.86 * voltage * voltage + 857.39 * voltage) * 0.5;
  
  Serial.print("TDS: ");
  Serial.print(tdsValue);
  Serial.println(" ppm");
}


void OD() {
  int sensorValue = analogRead(analogPinOD);
  
   oxigeno = -0.0011 * pow(sensorValue * (5.0 / 1023.0) * 1000.0, 2) + 0.1771 * (sensorValue * (5.0 / 1023.0) * 1000.0) + 0.0067;

  // Imprime los valores en el monitor serial
  Serial.print("  OD: ");
  Serial.println(oxigeno);
  //Serial.print("  Voltaje (mV): ");
  //Serial.println(sensorValue * (5.0 / 1023.0) * 1000.0, 2);

  // Control de las bombas de aire

  if (oxigeno < 7.0) {
    digitalWrite(bombaAire1Pin, HIGH); // Activa bomba 1
  } else {
    digitalWrite(bombaAire1Pin, LOW);  // Apaga bomba 1
  }


  if (oxigeno < 4.5) {
    digitalWrite(bombaAire2Pin, HIGH); // Activa bomba 2
  } else {
    digitalWrite(bombaAire2Pin, LOW);  // Apaga bomba 2
  }


  // Control de la bomba peristáltica
  unsigned long tiempoActual = millis();
  if (tiempoActual - ultimoTiempoDosis >= intervaloDosis) {
    // Activa bomba peristáltica
    unsigned long tiempoBomba = dosisMl * tiempoPorMl * 1000;
    digitalWrite(bombaPeristalticaPin, HIGH);
    delay(tiempoBomba);
    digitalWrite(bombaPeristalticaPin, LOW);

    // Actualiza el tiempo de la última dosis
    ultimoTiempoDosis = tiempoActual;
  }
  delay(1000);
}


void PH() {
  for (int i = 0; i < 10; i++) {
    buffer_arr[i] = analogRead(A4);
    delay(500);
  }
  for (int i = 0; i < 9; i++) {
    for (int j = i + 1; j < 10; j++) {
      if (buffer_arr[i] > buffer_arr[j]) {
        temp = buffer_arr[i];
        buffer_arr[i] = buffer_arr[j];
        buffer_arr[j] = temp;
      }
    }
  }

  avgval = 0;
  for (int i = 2; i < 8; i++) {
    avgval += buffer_arr[i];
  }
  float volt = (float)avgval * 5.0 / 1024 / 6;
   ph_act = -5.70 * volt + calibration_value;

  Serial.print("Voltaje:");
  Serial.println(volt);
  
  Serial.print("Nivel de PH:");
  Serial.println(ph_act);
}


void medirNivelAgua() {
  lectura5 = analogRead(nivel5);
  lectura20 = analogRead(nivel20);
  lectura40 = analogRead(nivel40);
  lectura60 = analogRead(nivel60);
  lectura80 = analogRead(nivel80);
  lectura100 = analogRead(nivel100);

  // Determinar el nivel de agua
  if (lectura100 >= umbral100) {
    nivelTanque = 100;
    Serial.println("Nivel 100%");
  } else if (lectura80 >= umbral80) {
    nivelTanque = 80;
    Serial.println("Nivel 80%");
  } else if (lectura60 >= umbral60) {
    nivelTanque = 60;
    Serial.println("Nivel 60%");
  } else if (lectura40 >= umbral40) {
    nivelTanque = 40;
    Serial.println("Nivel 40%");
  } else if (lectura20 >= umbral20) {
    nivelTanque = 20;
    Serial.println("Nivel 20%");
  }  else if (lectura5 >= umbral5) {
    nivelTanque = 5;
    Serial.println("Nivel 10%");
  }   else {
    nivelTanque = 0;
    Serial.println("Nivel 0%");
  }

  Serial.print("Nivel del tanque almacenado: ");
  Serial.println(nivelTanque);

  Serial.print("Lectura nivel 10%: ");
  Serial.print(lectura5);
  Serial.print("Lectura nivel 20%: ");
  Serial.print(lectura20);
  Serial.print(" | Nivel 40%: ");
  Serial.print(lectura40);
  Serial.print(" | Nivel 60%: ");
  Serial.print(lectura60);
  Serial.print(" | Nivel 80%: ");
  Serial.print(lectura80);
  Serial.print(" | Nivel 100%: ");
  Serial.println(lectura100);

  delay(1000); // Lectura 1 segundos
}


void LORA() {
  // Enviar datos vía LoRa sin limitantes de caracteres
  /*LoRa.beginPacket();

  LoRa.print(temperatureT);
  LoRa.println(" °C");

  LoRa.print(temperatureF);
  LoRa.println(" °F");

  LoRa.print(tdsValue);
  LoRa.println(" ppm");

  LoRa.print(oxigeno);
  LoRa.println(" mg/L");

  LoRa.print("PH: ");
  LoRa.println(ph_act);

  LoRa.print("Nivel: ");
  LoRa.println(nivelTanque);

  LoRa.endPacket();*/


  Serial.print(temperatureT);
  Serial.print(",");
  Serial.print(temperatureF);
  Serial.print(",");
  Serial.print(tdsValue);
  Serial.print(",");
  Serial.print(oxigeno);
  Serial.print(",");
  Serial.print(ph_act);
  Serial.print(",");
  Serial.println(nivelTanque);
  


  LoRa.beginPacket();

  LoRa.print(temperatureT);
  LoRa.print(",");
  LoRa.print(temperatureF);
  LoRa.print(",");
  LoRa.print(tdsValue);
  LoRa.print(",");
  LoRa.print(oxigeno);
  LoRa.print(",");
  LoRa.print(ph_act);
  LoRa.print(",");
  LoRa.print(nivelTanque);
  
  LoRa.endPacket();

  // Esperar 5 segundos antes de enviar nuevamente
  delay(5000);
}
