/*
*** ChronobusV4.2 *** 02/12/2017
JB Bailly
**

 */

// include the library code:
#include <LiquidCrystal.h>
#include <ESP8266WiFi.h>
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>

// pour faire la conversion avec les pin du NodeMCU (N1, N2, ..) et les numeros GPI0 mappes (5,4, ...)
static const int N0= 16;
static const int N1   = 5;
static const int N2   = 4;
static const int N3   = 0; //pin N3
static const int N4   = 2;
static const int N5   = 14;
static const int N6   = 12;
static const int N7   = 13;
static const int N8   = 15;  //pin N8
static const int N9   = 3;
static const int N10  = 1;

// initialize the library with the numbers of the interface pins
//LiquidCrystal(rs, enable, d4, d5, d6, d7) --> pin cote écran
LiquidCrystal lcd(N1, N2, N4, N5, N6, N7); // --> Pin coté NodeMcu


// Wi-Fi Settings
const char* ssid = "XXXX"; //entrer votre SSID
const char* password = "XXXX"; //entrer votre mot de passe wifi

WiFiClientSecure client; //permet la connexion en HTTPS
const unsigned long HTTP_TIMEOUT = 10000;  // max respone time from server
const size_t MAX_CONTENT_SIZE = 512;       // max size of the HTTP response

// Permet la gestion multiTaches
unsigned long currentMillis = 0;    // stores the value of millis() in each iteration of loop()
unsigned long dernierPressBouttonMillis = 0;
const int bouttonInterval = 300 ;

//valeurs liées aux 2 boutons permettant 
int bouttonPresse =0;
int changeTransp = 0;
int indexTransp = 0;
unsigned long dernierUp =0;
unsigned long IntervalUp = 15000;
unsigned long dernierAff =0;
unsigned long IntervalAff = 350;

const int bouttonPin = N3; // the pin number for the button

//Adresse Bus Stop V3.
const char* host = "api-ratp.pierre-grimaud.fr";  // server's address
const String  adressD = "/v3/schedules/";
const String  adressF = "?_format=json";
char*  transport[]= {"tramways/3b" , "bus/96"} ;
char*  station[] = {"porte+des+lilas", "porte+des+lilas+metro"};
char*  sens[] = {"A" , "R"};
//exemple  de requêtes envoyées:
// const char* resource = "/v3/schedules/bus/96/porte+des+lilas+metro/R?_format=json";    // bus 96 direction Sud
// const char* resource = "/v3/schedules/tramways/3b/porte+des+lilas/A?_format=json";   // Tram 3b DIR: Porte de la chapelle
const char*  prochains[2];
const char*  dest[2];
String destAff[2] ;
int indexAff=0;

String jsonEnd;
String readString;


void setup() {

  Serial.begin(9600); //vitesse de l'esp12
  Serial.println("test liaison série");

  // set up the LCD's number of columns and rows:
 lcd.begin(16, 2);
 lcd.setCursor(0, 0 ); 
 lcd.print("initialisation");

  //boucle pour connexion WIFI
  WiFi.begin(ssid, password);
 while (WiFi.status() != WL_CONNECTED) {
 delay(500);
  }
  
// Multitache
//set the button pin as input
pinMode(bouttonPin, INPUT);

 }

void loop() {

//******************************
//Boucle principale
//******************************

  currentMillis = millis();   // capture the latest value of millis()
  readButton();  		//on teste si le bouton a été pressé

//on vérifie si le bouton est préssé et on met la va valeur "changeTransp" à 1 pour l'indiquer pour la suite
//et on change l'index de transport
  if (bouttonPresse == 1) {
    Serial.print("changement transp");
    changeTransp = 1;
    indexTransp += 1 %2 ;
    bouttonPresse =0;
  } 

if ((currentMillis - dernierUp >= IntervalUp) || (changeTransp == 1) ) {
  //on construit la requête suivant 'indexTransp" 
  // !! POUR L'INSTANT CHANGE AUSSI LE SENS !
  //Update à faire pour rajouter un bouton "sens"
  if (connect(host)) {
    String  ressourceS = adressD + transport[indexTransp] + '/' + station[indexTransp] + '/' + sens[indexTransp] + adressF ;
    char resource[ressourceS.length() +1 ];
    ressourceS.toCharArray(resource, (ressourceS.length()+1) ) ;
    Serial.println("connecte");         
    sendRequest(host, resource);        //envoi de la requete a HOST pour receuillir le contenu de RESSOURCE
    Serial.println("requete envoyee");
    String jsonEXT= readClient();       //Enregistrement de la reponse + extrait du JSON dans jsonEXT
    JsonObject& data = readReponseContent(jsonEXT);        //Parsing du jsonEXT --> root
    extraction(data);
                    
    dernierUp = currentMillis +10;
    changeTransp =0 ;
  }
 disconnect();
 }
 if ((currentMillis - dernierAff >= IntervalAff)  ) {
  affichage();
  dernierAff= currentMillis +10;   
  }
}
//******************************
//Les fonctions appellées dans la boucle principale
//******************************

// Open connection to the HTTP server "host"
bool connect(const char* hostName) {
  Serial.print("Connect to ");
  Serial.println(hostName);

  bool ok = client.connect(hostName, 443);

  Serial.println(ok ? "Connected" : "Connection Failed!");
  return ok;
}

// Send the HTTP GET request to the server "ressource"
bool sendRequest(const char* host, const char* resource) {
  Serial.print("GET ");
  Serial.println(resource);
  client.print("GET ");
  client.print(resource);
  client.println(" HTTP/1.1");
  client.print("Host: ");
  client.println(host);
  client.println("Connection: close");
  client.println();


  return true;
}

//Recuperation de la reponse tant que client connecte et données présentes dans le buffer (String c)
String readClient() {
  while(client.connected() && !client.available()) delay(1);   //waits for data
  while (client.connected() || client.available()) {           //connected or data available
    
    char c = client.read(); //gets byte from ethernet buffer
    readString += c; //places captured byte in readString
  }
  int debJ = readString.indexOf('{');
  int endJ = readString.lastIndexOf('}');
  jsonEnd=readString.substring(debJ,endJ+1);
  readString="";
    return jsonEnd;
}
  
//Parsing du Strin "c" dans le JSON "root"
JsonObject& readReponseContent(String json) {

  const size_t BUFFER_SIZE = JSON_ARRAY_SIZE(2) 
    + JSON_OBJECT_SIZE(1) 
  + 3*JSON_OBJECT_SIZE(2) 
  + JSON_OBJECT_SIZE(3) 
  + 230;
  
  // Allocate a temporary memory pool
  DynamicJsonBuffer jsonBuffer(BUFFER_SIZE);
  JsonObject& root = jsonBuffer.parseObject(json);

  if (!root.success()) {
    Serial.println("JSON parsing failed!");
  //  return false;
  }
  return root ;
}

void extraction(JsonObject& json) {
  // Extraits des 2 prochains passages
prochains[0]= json["result"]["schedules"][0]["message"]; 
prochains[1]= json["result"]["schedules"][1]["message"];

dest[0]= json["result"]["schedules"][0]["destination"]; 
dest[1]= json["result"]["schedules"][1]["destination"]; 

destAff[0]= dest[0];
destAff[1]= dest[1];

}


// Print the data extracted from the JSON
void affichage() {

  //affichage serie
  Serial.print("proch = ");
  Serial.println(prochains[0]);
  Serial.print("suiv = ");
  Serial.println(prochains[1]);
  
  //envoi sur ecran LCD
  lcd.clear();
  for (int i = 0; i < 2; i++) { 
    lcd.setCursor(0, i ); 
    lcd.print(prochains[i]);
    }

  //coupe de la destination
  
  for (int i = 0; i < 2; i++) { 
    lcd.setCursor(6, i ); 
      if (destAff[i].length() == 0) {
       destAff[i]= dest[i];
       }  
    String barAffich =  "|" + destAff[i] ;
    Serial.println(barAffich);
    lcd.print( barAffich);
    destAff[i].remove(0,1) ;
    }

}


void readButton() {
  
  if (millis() - dernierPressBouttonMillis >= bouttonInterval) {

    if (digitalRead(bouttonPin) == HIGH) {
      Serial.print("boutton presse");
      bouttonPresse = 1; 
      dernierPressBouttonMillis  = currentMillis +10;
    }
  }

}

// Close the connection with the HTTP server
void disconnect() {
  Serial.println("Deconnecte");
  client.stop();
}


