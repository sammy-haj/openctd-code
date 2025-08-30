#include <Wire.h>       // Communication I2C
#define EC_ADDRESS 100  // Adresse I2C par défaut pour la sonde EC EZO

#include "TSYS01.h"     // Capteur de température
#include "MS5837.h"     // Capteur de pression/profondeur

#include <SPI.h>
#include <SD.h>         // Carte SD
#include <RTClib.h>     // Horloge temps réel (RTC)

#define SD_CS 10        // Pin CS pour la carte SD

File Data_File;

TSYS01 sensor;          // Capteur de température TSYS01
MS5837 sensor2;         // Capteur de pression MS5837
RTC_PCF8523 rtc;        // RTC utilisé (PCF8523)

char ec_data[32];       // Données reçues de la sonde EC
byte in_char = 0;
byte i = 0;
int time_ = 1000;       // Temps d’attente pour lecture après commande EC

void setup() {
  Serial.begin(9600);
  Wire.begin();
  delay(1000);

  Serial.println("Init...");

  // Initialisation de l'horloge RTC
  if (! rtc.begin()) {
    Serial.println("Couldn't find RTC");
    Serial.flush();
    while (1) delay(10);
  }

  // Si l’horloge a perdu l’alimentation, on la remet à l’heure actuelle
  if (! rtc.initialized() || rtc.lostPower()) {
    Serial.println("RTC is NOT initialized, let's set the time!");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  rtc.start();
  Serial.println("RTC OK");
  
  // Initialisation de la carte SD
  Serial.print("Initialisation de la carte SD...");
  if (!SD.begin(SD_CS)) {
    Serial.println("Échec de l'initialisation !");
    while(1){
      digitalWrite(13,HIGH); 
      delay(500);   
      digitalWrite(13,LOW); 
      delay(500);
    }
    exit(1);
  }
  Serial.println("Carte SD initialisée.");

  // Création du fichier de données
  Serial.println("Création du fichier data.txt...");
  Data_File = SD.open("data.txt", FILE_WRITE);

  if (Data_File) {
    Serial.println("Fichier créé avec succès.");
    Data_File.println("Date, Heure, Temperature, Pression, Profondeur, Altitude, Conductivite, Salinite");
    Data_File.close();
  } else {
    Serial.println("Erreur lors de la création du fichier !");
  }

  // Configuration de la sonde EC : désactivation des sorties inutiles, activation de EC et Salinité
  sendECCommand("O,SG,0");
  sendECCommand("O,EC,0");
  sendECCommand("O,TDS,0");
  sendECCommand("O,SAL,SG,0");
  sendECCommand("O,EC,1");
  sendECCommand("O,S,1");

  // Initialisation du capteur de température
  while (!sensor.init()) {
    Serial.println("TSYS01 device failed to initialize!");
    delay(2000);
  }
  Serial.println("TSYS01 OK");

  // Initialisation du capteur de pression
  while (!sensor2.init()) {
    Serial.println("Init failed!");
    Serial.println("Are SDA/SCL connected correctly?");
    Serial.println("Blue Robotics Bar30: White=SDA, Green=SCL\n\n\n");
    delay(5000);
    sensor2.setFluidDensity(997);  // Densité du fluide (997 pour eau douce)
  }
  Serial.println("MS5837 OK");
}

void loop() {
  Data_File = SD.open("data.txt", FILE_WRITE);  // Réouverture du fichier pour ajouter les données
  DateTime now = rtc.now();                     // Récupération de la date/heure actuelle
  float ec_float, tds_float, sal_float, sg_float;

  sensor.read();     // Lecture capteur température
  sensor2.read();    // Lecture capteur pression

  // Envoi de la température actuelle à la sonde EC
  char tempStr[10];
  sprintf(tempStr, "%.2f", sensor.temperature());  
  char command[20];
  snprintf(command, sizeof(command), "RT,%s", tempStr);  
  sendECCommand(command);

  // Extraction des valeurs EC et salinité
  parseECData(ec_float, sal_float);

  // Écriture des données dans le fichier
  Data_File.print(now.day()); Data_File.print("/");
  Data_File.print(now.month()); Data_File.print("/");
  Data_File.print(now.year()); Data_File.print(", ");

  Data_File.print(now.hour()); Data_File.print(":");
  Data_File.print(now.minute()); Data_File.print(":");
  Data_File.print(now.second()); Data_File.print(", ");

  Data_File.print(sensor.temperature()); Data_File.print(", ");
  Data_File.print(sensor2.pressure()); Data_File.print(", ");
  Data_File.print(sensor2.depth()); Data_File.print(", ");
  Data_File.print(sensor2.altitude()); Data_File.print(", ");
  Data_File.print(ec_float); Data_File.print(", ");
  Data_File.print(sal_float); Data_File.print("\n");

  Data_File.flush();  // Sauvegarde immédiate
  Data_File.close();

  delay(400);  // Pause entre les mesures (peut être ajustée)
}

// Fonction pour envoyer une commande à la sonde EC
void sendECCommand(const char *command) {
  Wire.beginTransmission(EC_ADDRESS);
  Wire.write(command);
  Wire.endTransmission();
  delay(time_);  // Attente pour réponse

  Wire.requestFrom(EC_ADDRESS, 32, 1);
  byte code = Wire.read();  // Lecture du code de réponse

  i = 0;
  while (Wire.available()) {
    in_char = Wire.read();
    ec_data[i] = in_char;
    i++;
    if (in_char == 0) {
      i = 0;
      break;
    }
  }
}

// Fonction pour extraire EC et salinité depuis la réponse de la sonde
void parseECData(float &ec_float, float &sal_float) {
  char *ec = strtok(ec_data, ",");
  char *sal = strtok(NULL, ",");

  if (ec) ec_float = atof(ec);
  if (sal) sal_float = atof(sal);
}
