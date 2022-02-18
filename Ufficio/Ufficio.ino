/*
  LoRa Simple Gateway/Node Exemple

  This code uses InvertIQ function to create a simple Gateway/Node logic.

  Gateway - Sends messages with enableInvertIQ()
          - Receives messages with disableInvertIQ()

  Node    - Sends messages with disableInvertIQ()
          - Receives messages with enableInvertIQ()

  With this arrangement a Gateway never receive messages from another Gateway
  and a Node never receive message from another Node.
  Only Gateway to Node and vice versa.

  This code receives messages and sends a message every second.

  InvertIQ function basically invert the LoRa I and Q signals.

  See the Semtech datasheet, http://www.semtech.com/images/datasheet/sx1276.pdf
  for more on InvertIQ register 0x33.

  created 05 August 2018
  by Luiz H. Cassettari
*/

// APRI = 0x01
// CHUDI = 0x02
// ACK = 0x3

#include <SPI.h> // include libraries
#include <LoRa.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27, 20, 4);

#define NUM_VALVOLE 3

#define VALVOLA1 A0
#define VALVOLA2 A1
#define VALVOLA3 A2
#define VALVOLA4 A3
#define VALVOLA5 8
#define VALVOLA6 7
#define VALVOLA7 6
#define VALVOLA8 5
#define VALVOLA9 4
#define VALVOLA10 3

byte UFFICIO = 0x11;

byte APRI = 0x01;
byte CHIUDI = 0x02;
byte ACKAPERTO = 0x03;
byte ACKCHIUSO = 0x04;

#define INTERVALLO_WAIT 5000

bool ReceivedAck = true;

byte destination = 0xAA;  // destinazione, sempre la valvola 1 sul stradone
byte localAddress = 0x11; //indirizzo ufficio
byte comando;             //comando da mandare (apri o chiudi)
byte valve;
;                  //indirizzo valvola da manipolare
byte finalAddress; //indirizzo finale nodo da manipolare, nell'ufficio sara sempre uguale a "valve"

int Array_val[] = {VALVOLA1, VALVOLA2, VALVOLA3, VALVOLA4, VALVOLA5, VALVOLA6, VALVOLA7, VALVOLA8, VALVOLA9, VALVOLA10};

int ReturnValues[NUM_VALVOLE] = {2};
bool isOpen[NUM_VALVOLE] = {false};
bool isClosed[NUM_VALVOLE] = {false};
unsigned long millisOPEN[NUM_VALVOLE], millisCLOSE[NUM_VALVOLE];
int millisCountOpen[NUM_VALVOLE] = {0}, millisCountClose[NUM_VALVOLE] = {0};

String message = "";

byte ValveAddress(int i)
{
  byte Address[] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE};

  return Address[i];
}

void checkINPUT(int Input_Val[], int *ReturnValues)
{
  for (int i = 0; i < NUM_VALVOLE; i++)
  {
    if (Input_Val[i] == 0)
    {
      if (millis() - millisOPEN[i] < 100)
      {
        millisCountOpen[i]++;
        millisOPEN[i] = millis();
        millisCountClose[i] = 0;
      }
      millisOPEN[i] = millis();
    }
    else if (Input_Val[i] == 1)
    {
      if (millis() - millisCLOSE[i] < 30)
      {
        millisCountClose[i]++;
        millisCLOSE[i] = millis();
      }
      millisCLOSE[i] = millis();
    }
  }

  for (int j = 0; j < NUM_VALVOLE; j++)
  {
    if (millisCountOpen[j] == 20)
    {
      millisCountOpen[j] = 0;
      ReturnValues[j] = 1;
      continue;
    }
    if (millisCountClose[j] == 30)
    {
      millisCountClose[j] = 0;
      ReturnValues[j] = 0;
      continue;
    }

    ReturnValues[j] = 2;
  }
}

void setup()
{
  lcd.init(); // initialize the lcd
  lcd.backlight();

  for (int i = 0; i < NUM_VALVOLE; i++) //iniazializza pin valvole in uso
  {
    pinMode(Array_val[i], INPUT_PULLUP);
  }

  Serial.begin(9600); // initialize serial
  while (!Serial)
    ;

  if (!LoRa.begin(433E6))
  {
    Serial.println("LoRa init failed. Check your connections.");
    while (true)
      ; // if failed, do nothing
  }

  Serial.println("LoRa init succeeded.");
  Serial.println();
  Serial.println("Nodo Ufficio");
  Serial.println("Questo è un nodo");
  Serial.println("Tx: invertIQ disable");
  Serial.println("Rx: invertIQ enable");
  Serial.println();

}

void loop()
{

  int Input_Val[NUM_VALVOLE];
  for (int i = 0; i < NUM_VALVOLE; i++)
  {
    Input_Val[i] = digitalRead(Array_val[i]);
  }

  checkINPUT(Input_Val, ReturnValues);

  for (int i = 0; i < NUM_VALVOLE; i++)
  {
    if (ReturnValues[i] == 1 && isOpen[i] == false)
    {

      isClosed[i] = false;
      //   message = "O" + String(i);

      comando = APRI;
      valve = ValveAddress(i);
      finalAddress = ValveAddress(i);

      lcd.clear();
      lcd.setCursor(1, 0);
      lcd.print("Apro valvola N: " + String(i + 1));
      lcd.setCursor(0, 2);
      lcd.print(".....");
      delay(200);

      if (SendAndWaitAck())

        lcd.setCursor(0, 2);
      lcd.print("Aperto");
      isOpen[i] = true; //solo quando torna acknowledgement

      //aggiungere codice per aspettare risposta con timer o ripetere
    }
    else if (ReturnValues[i] == 0 && isClosed[i] == false)
    {
      isOpen[i] = false;
      //message = "C" + String(i);

      comando = CHIUDI;
      valve = ValveAddress(i);
      finalAddress = ValveAddress(i);

      lcd.clear();
      lcd.setCursor(1, 0);
      lcd.print("Chiudo valvola N: " + String(i + 1));
      lcd.setCursor(0, 2);
      lcd.print(".....");
      delay(200);

      if (SendAndWaitAck())

        lcd.setCursor(0, 2);
      lcd.print("Chiuso");
      isClosed[i] = true; //solo quando torna acknowledgement

      //aggiungere codice per aspettare risposta con timer o ripetere
    }
  }
}

int SendAndWaitAck() //bisogna aggiungere un timeout
{
  ReceivedAck = false;
  unsigned long previousMillis = 0;

  while (!ReceivedAck)
  {
    if (millis() - previousMillis >= INTERVALLO_WAIT) //ogni 3 secondi rimanda il messaggio;
    {
      previousMillis = millis();
      SendMessage();
    }
    onReceive(LoRa.parsePacket());
  }

  return 1;
}


void SendMessage()
{
  Serial.println("Sending packet: ");
  Serial.println("comando:" + String(comando));
  Serial.println("Valve:" + String(valve));
  LoRa.beginPacket();       // start packet
  LoRa.write(destination);  // add destination address
  LoRa.write(localAddress); // add sender address
  LoRa.write(comando);
  LoRa.write(valve);
  LoRa.write(finalAddress);
  LoRa.endPacket(); // finish packet and send it
}



void onReceive(int packetSize)
{
  // String Ack = "";

  // while (LoRa.available())
  // {
  //     Ack += (char)LoRa.read();
  // }

  // Serial.print("Ufficio ha ricevuto: ");
  // Serial.println(Ack);
  // String LowerCaseMessage = message;
  // LowerCaseMessage.toLowerCase();
  // if (Ack.equals("r" + LowerCaseMessage))
  // {
  //     ReceivedAck = true;
  // }

  if (packetSize == 0)
    return; // if there's no packet, return

  // read packet header bytes:
  byte recipient = LoRa.read(); // recipient address
  byte sender = LoRa.read();    // sender address
  byte response = LoRa.read();  // ack di risposta
  byte AckValve = LoRa.read();
  byte AckFinalAddress = LoRa.read(); // incoming msg length

  String incoming = "";

  while (LoRa.available())
  {
    incoming += (char)LoRa.read();
  }

  if (incoming.length() > 0)
  { // check length for error
    Serial.println("error: message length does not match length");
    return; // skip rest of function
  }

  if (recipient != localAddress && recipient != 0xFF)
  {
    Serial.println("This message is not for me.");
    return; // skip rest of function
  }

  Serial.println("UFFICIO received from: 0x" + String(sender, HEX));
  Serial.println("Sent to: 0x" + String(recipient, HEX));
  Serial.println("comando: 0x" + String(response, HEX));
  Serial.println("Per valvola N. 0x" + String(AckValve, HEX));
  Serial.println("Con destinazione finale:  0x" + String(AckFinalAddress, HEX));
  Serial.println("RSSI: " + String(LoRa.packetRssi()));
  Serial.println("Snr: " + String(LoRa.packetSnr()));
  Serial.println();

  if (AckValve == valve || AckFinalAddress == localAddress)
  {
    if (comando == APRI)
    {
      if (response == ACKAPERTO)
        ReceivedAck = true;
    }
    else if (comando == CHIUDI)
    {
      if (response == ACKCHIUSO)
        ReceivedAck = true;
    }
  }

}