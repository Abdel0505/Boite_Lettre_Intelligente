#include <SPI.h>               // Gère la communication SPI utilisée par le module LoRa
#include <LoRa.h>              // Bibliothèque pour utiliser le module LoRa
#include <Wire.h>              // Gère la communication I2C (utile pour la RTC)
#include <RTClib.h>            // Bibliothèque pour communiquer avec la RTC DS3231
#include <NewPing.h>           // Bibliothèque pour simplifier l’usage du capteur ultrason HC-SR04

#define TRIGGER_PIN A3          // Pin utilisée pour envoyer le signal du capteur
#define ECHO_PIN A2             // Pin utilisée pour recevoir l’écho du capteur
#define MAX_DISTANCE 50        // Distance maximale mesurable par le capteur en cm

NewPing sonar(TRIGGER_PIN, ECHO_PIN, MAX_DISTANCE);  // Création d’un objet capteur
RTC_DS3231 rtc;               // Création d’un objet pour la RTC

float distanceInitiale = 0;    // Distance mesurée au démarrage pour référence (sol de la boîte vide)
unsigned long lastMeasureTime = 0; // Moment de la dernière mesure
const unsigned long interval = 10000; // Intervalle entre chaque mesure (10 secondes)

int confirmationsLettre = 0;   // Compteur de confirmations consécutives pour une lettre
int confirmationsColis = 0;    // Compteur de confirmations consécutives pour un colis
bool messageEnvoye = false;    // Pour éviter d’envoyer plusieurs fois le même message

void setup() {
  Serial.begin(115200);          // Initialisation de la communication série pour debug
  while (!Serial);             // Attente que le port série soit prêt

  if (!LoRa.begin(868E6)) {    // Démarrage du LoRa à 868 MHz (adapter si besoin)
    Serial.println("Échec de l'initialisation LoRa");
    while (1);                 // Bloque le programme si LoRa ne démarre pas
  }

  if (!rtc.begin()) {          // Vérifie si la RTC est détectée
    Serial.println("RTC non trouvée → mode sans contrainte d'heure");
  }

  delay(1000);                 // Petite pause pour laisser le capteur se stabiliser
  distanceInitiale = sonar.ping_cm();  // Enregistre la distance vide (boîte sans objet)
  Serial.print("Distance initiale enregistrée : ");
  Serial.print(distanceInitiale);
  Serial.println(" cm");
}

void loop() {
  unsigned long currentTime = millis();  // Temps actuel depuis le démarrage

  if (currentTime - lastMeasureTime >= interval) { // Si 10 secondes sont passées
    lastMeasureTime = currentTime;                // Met à jour l’horodatage

    float distanceActuelle = sonar.ping_cm();     // Mesure actuelle de distance
    float ecart = distanceInitiale - distanceActuelle; // Différence par rapport à la boîte vide

    Serial.print("Distance actuelle : ");
    Serial.print(distanceActuelle);
    Serial.print(" cm → Écart : ");
    Serial.print(ecart);
    Serial.println(" cm");

    // Si l’écart est très faible, on considère que la boîte est vide : on réinitialise tout
    if (ecart < 0.2) {
      confirmationsLettre = 0;   // Réinitialisation du compteur lettre
      confirmationsColis = 0;    // Réinitialisation du compteur colis
      messageEnvoye = false;     // Autorise un prochain envoi
    }

    // Si l’écart est supérieur à 5 cm : possible colis
    else if (ecart > 5.0 && !messageEnvoye) {
      confirmationsColis++;     // On augmente le compteur colis
      confirmationsLettre = 0;  // On remet à 0 le compteur lettre

      if (confirmationsColis >= 3) {           // Si on a 3 confirmations successives
        LoRa.beginPacket();                    // On prépare un paquet LoRa
        LoRa.print("Colis détecté");           // Contenu du message
        LoRa.endPacket();                      // Envoi du message
        Serial.println("Message LoRa envoyé : Colis détecté");
        messageEnvoye = true;                  // On bloque les futurs envois
      }
    }

    // Si l’écart est entre 0.4 et 5 cm : possible lettre
    else if (ecart > 0.4 && ecart <= 5.0 && !messageEnvoye) {
      confirmationsLettre++;     // On augmente le compteur lettre
      confirmationsColis = 0;    // On remet à 0 le compteur colis

      if (confirmationsLettre >= 3) {          // Si 3 confirmations successives
        LoRa.beginPacket();                    // Préparation du message
        LoRa.print("Lettre détectée");         // Contenu du message
        LoRa.endPacket();                      // Envoi du message
        Serial.println("Message LoRa envoyé : Lettre détectée");
        messageEnvoye = true;                  // Bloque les futurs envois
      }
    }
  }
}
