#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <ArduinoJson.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <TimeLib.h>
#include <time.h>
#include <vector>


const char* ssid = "sauerkraut"; // Ersetze mit deinem WLAN-Namen
const char* password = "Gasse_123."; // Ersetze mit deinem WLAN-Passwort
WiFiClientSecure client;
HTTPClient http;
String serverName = "https://www.hvakosterstrommen.no/api/v1/prices/";
WiFiUDP udp;
NTPClient ntpClient(udp, "pool.ntp.org", 3600, 60000); // UTC+1 Stunde, Aktualisierung alle 60 Sekunden
String priceArea = "NO3"; //prisområde
//int cheapest[18] = {0};
const int relay_pin = D1; // D1 = GPIO 5
bool preiseAktuell = false;
bool relay_state = LOW;
bool previous_relay_state = LOW;
std::vector<int> cheapest (18, 0); // Globaler Vector mit Grøsse 18 und Werte 0 fuer Guenstigste Preise
int oldHour = 0;


void setTimeFromNTP() {
    ntpClient.update();
    unsigned long epochTime = ntpClient.getEpochTime();
    // Setze die Zeit
    setTime((time_t)epochTime); // Konvertiere epochTime zu time_t
}

std::vector<int> getDataAndSort(String url){     //function zum parsen und sortieren der Daten // Wifi-Verfuegbarkeit wird ueber Loop abgefangen

  std::vector<float> prices(24, 0); // Neuer Vector "prices" mit Grøsse 24 und Werte 0
  std::vector<int> hours(24, 0); // Neuer Vector "hours" mit Grøsse 24 und Werte 0
  //float prices[24] = {0}; // Initialisieren Array zum Einschreiben der Preise
  //static int hours[24] = {0};
  client.setInsecure(); // Deaktiviert die Zertifikatsüberprüfung
  http.useHTTP10(true);

  while (true) {
    http.begin(client, url);
    int httpCode = http.GET();

      if (httpCode > 0) { // Überprüfen, ob die Anfrage erfolgreich war
        if (httpCode == HTTP_CODE_OK) {
        // Parse response
         DynamicJsonDocument doc(2048);
         DeserializationError error = deserializeJson(doc, http.getStream());

          if (!error) {
            for (size_t i = 0; i < doc.size(); i++) {
            prices[i] = doc[i]["NOK_per_kWh"];
            }

          ///^^^^^^^^^^^^^^^^^^ SORTIERUNG ^^^^^^^^^^^^^^^^^^///

          // Initialisiere das Indizes-Array 
          for (int i = 0; i < doc.size(); i++) {
          hours[i] = i; // Setze jeden Index auf seine ursprüngliche Position
          }

          for (int i = 0; i < doc.size() - 1; i++) {        
          int minIndex = i;
            for (int j = i + 1; j < doc.size(); j++) {
              if (prices[j] < prices[minIndex]) {
              minIndex = j;
              }
            }
          // Tausche die gefundenen kleinsten Werte
          float temp = prices[i];
          prices[i] = prices[minIndex];
          prices[minIndex] = temp;

          // Tausche die Indizes entsprechend
          int tempIndex = hours[i];
          hours[i] = hours[minIndex];
          hours[minIndex] = tempIndex;
          }
          ///^^^^^^^^^^^^^^^^^^ SORTIERUNG ^^^^^^^^^^^^^^^^^^///

          } else {
                    Serial.println("Fehler beim Parsen der JSON-Daten");
            }
        } else {
                Serial.printf("HTTP-Fehler: %d\n", httpCode);
          }
  break; // Verlasse die Schleife, wenn die Anfrage erfolgreich war
  } else {
            Serial.printf("Fehler bei der HTTP-Anfrage: %s\n", http.errorToString(httpCode).c_str());
            Serial.println("API nicht erreichbar. Versuche es erneut in 5 Sekunden...");
            delay(5000); // Warte 5 Sekunden, bevor du es erneut versuchst
    }

http.end(); // Schließe die Verbindung
}

return hours;
}

String makeURL(){  
  // Aktuelle Zeit abrufen
  ntpClient.update();
  
  // Jahr, Monat und Tag abrufen
  unsigned long epochTime = ntpClient.getEpochTime();
  time_t rawTime = (time_t)epochTime; // Umwandlung in time_t
  struct tm * timeinfo = localtime(&rawTime);

  int year = timeinfo->tm_year + 1900; // Jahr (tm_year ist Jahre seit 1900)
  int month = timeinfo->tm_mon + 1;     // Monat (tm_mon ist 0-11)
  int day = timeinfo->tm_mday;          // Tag

  // Monat und Tag in zweistelliges Format bringen
  String monthStr = (month < 10) ? "0" + String(month) : String(month);
  String dayStr = (day < 10) ? "0" + String(day) : String(day);

  String url = serverName + year + "/" + monthStr + "-" + dayStr + "_" + priceArea + ".json";
  //Serial.println(url);
  return url;
}

void setup(){
  Serial.begin(9600); 
  pinMode(relay_pin, OUTPUT);
  WiFi.begin(ssid, password);
  Serial.println("Connecting");
  while(WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  ntpClient.begin();  // evtl. crosscheck if worked?
  setTimeFromNTP();
  Serial.println("");
  Serial.print("Connected to WiFi network with IP Address: ");
  Serial.println(WiFi.localIP());
}

void printTime(){
Serial.print(day());
Serial.print(".");
Serial.print(month());
Serial.print(".");
Serial.print(year());
Serial.print(", ");
Serial.print(hour());
Serial.print(":");
Serial.print(minute());
Serial.print(":");
Serial.print(second());
Serial.print(" :   ");
}


void loop(){
    
int currentHour = hour();

//  if (currentHour != oldHour){ // print Time every hour
 //   printTime();
 //   oldHour = currentHour;
 // }

/////// Jeden Tag um 00:02 aufrufen; wenn kein WLAN vorhanden: Setup aufrufen. ///////////
  if(hour() == 0 && minute() == 2){
    
    if(WiFi.status() != WL_CONNECTED){ // Call setup() wenn kein WLAN; setup loopt bis WLAN wieder da ist.
      setup(); 
    }
    String url = makeURL();
    cheapest = getDataAndSort(url);
    Serial.print("Cheapest hours ");
    Serial.print(day());
    Serial.print(".");
    Serial.print(month());
    Serial.print(".");
    Serial.print(year());
    Serial.print(" : ");
    for(int i=0; i<18; i++){
      Serial.print(cheapest[i]);
      Serial.print(" ");
    }
    Serial.println("");
    delay(1000*60*1); // wait a minute for preventing multiple api calls
    Serial.println("Getting Data done;");
  } 

/////// RELAY schalten /////////////////////////////////////////////////////////////////
  for (int i = 0; i < 18; i++){
      if(currentHour == cheapest[i]){
      relay_state = LOW; // Relay aus = Boiler ein
      digitalWrite(relay_pin, relay_state);
        if (relay_state != previous_relay_state){
          printTime();
          Serial.print(" -- Relais geændert zu: ");
          Serial.print(relay_state ? "HIGH" : "LOW");
          Serial.println(" => Boiler ein");
          previous_relay_state = relay_state;
        }
      return;   // beendet loop-funktion!
      }
  } 
/////// RELAY schalten /////////////////////////////////////////////////////////////////

relay_state = HIGH; // relais ein = Boiler aus
digitalWrite(relay_pin, relay_state);
if (relay_state != previous_relay_state){
          printTime();
          Serial.print(" Relais geændert zu: ");
          Serial.print(relay_state ? "HIGH" : "LOW");
          Serial.println(" => Boiler aus");
          previous_relay_state = relay_state;
        }

}



