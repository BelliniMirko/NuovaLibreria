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
#include <SX127XLT.h>
#include "Settings.h"
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27, 20, 4);

SX127XLT LT;

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

uint8_t UFFICIO = 'A';

uint8_t APRI = 1;
uint8_t CHIUDI = 2;
uint8_t ACKAPERTO = 3;
uint8_t ACKCHIUSO = 4;

uint8_t TXPacketL;
uint8_t RXPacketL;  // length of received packet
int16_t PacketRSSI; // RSSI of received packet
int8_t PacketSNR;

#define INTERVALLO_WAIT 3000

bool ReceivedAck = true;

uint8_t destination = 'B';  // destinazione, sempre la valvola 1 sul stradone
uint8_t localAddress = 'A'; // indirizzo ufficio
uint8_t comando;            // comando da mandare (apri o chiudi)
uint8_t valve;              // indirizzo valvola da manipolare
uint8_t finalAddress;       // indirizzo finale nodo da manipolare, nell'ufficio sara sempre uguale a "valve"

int Array_val[] = {VALVOLA1, VALVOLA2, VALVOLA3, VALVOLA4, VALVOLA5, VALVOLA6, VALVOLA7, VALVOLA8, VALVOLA9, VALVOLA10};

int ReturnValues[NUM_VALVOLE] = {2};
unsigned long millisOPEN[NUM_VALVOLE], millisCLOSE[NUM_VALVOLE];
int millisCountOpen[NUM_VALVOLE] = {0}, millisCountClose[NUM_VALVOLE] = {0};

int isOpen[NUM_VALVOLE]; // keeps track of state of valves, 1 is open/ 0 is closed/ 2 is undefined

unsigned long timeOpen;

// bool isOpen[NUM_VALVOLE] = {false};
// bool isClosed[NUM_VALVOLE] = {false};

String convertMillis(unsigned long millis)
{
  unsigned long seconds = millis / 1000;
  unsigned long minutes = seconds / 60;
  unsigned long hours = minutes / 60;


  String time = String(hours%24) + ":" + String(minutes%60) + ":" + String(seconds%60);

  return time;
}

String returnOpen()
{
  String areOpen = "";
  bool first = true;
  for (int i = 0; i < NUM_VALVOLE; i++)
  {
    if (isOpen[i] == 1)
    {
      if (first)
      {
        areOpen = areOpen + String(i);
        first = false;
      }
      else
      {
        areOpen = areOpen + ", " + String(i);
      }
    }
  }

  if (areOpen == "")
  {
    areOpen = "Nessuna";
  }

  return areOpen;
}

uint8_t ValveAddress(int i)
{
  uint8_t Address[] = {'B', 'C', 'D', 'E', 'E', 'E', 'E', 'F'};

  return Address[i];
}

uint8_t Valve(int i)
{
  uint8_t Valve[] = {'B', 'C', 'D', 'E', 'F', 'G', 'H', 'F'};

  return Valve[i];
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
    if (millisCountClose[j] == 300)
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

  for (int i = 0; i < NUM_VALVOLE; i++) // iniazializza pin valvole in uso
  {
    pinMode(Array_val[i], INPUT_PULLUP);
  }

  for (int i = 0; i < NUM_VALVOLE; i++)
  {
    isOpen[i] = 2; // initialize array to value 2
  }

  Serial.begin(9600); // initialize serial
  while (!Serial)
    ;

  SPI.begin();

  if (LT.begin(NSS, NRESET, DIO0, LORA_DEVICE))
  {
    Serial.println("LoRa init succeeded.");
  }
  else
  {
    Serial.println(F("Device error"));
  }

  LT.setupLoRa(Frequency, Offset, SpreadingFactor, Bandwidth, CodeRate, Optimisation);

  Serial.println(F("Receiver ready"));
  Serial.println();
  Serial.println("LoRa init succeeded.");
  Serial.println();
  Serial.println("Nodo Ufficio");

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

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Aperte: " + returnOpen());

  if (returnOpen() != "Nessuna")
  {
    lcd.setCursor(0, 2);
    lcd.print("Da: " + convertMillis(millis() - timeOpen));
  }

  for (int i = 0; i < NUM_VALVOLE; i++)
  {
    if (ReturnValues[i] == 1 && isOpen[i] != 1)
    {

      comando = APRI;                 // comando da mandare
      valve = Valve(i);               // valvola a cui mandare il comando
      finalAddress = ValveAddress(i); // indirizzo finale del destinatario, ovvero la stazione che contiene quella valvola

      lcd.clear();
      lcd.setCursor(1, 0);
      lcd.print("Apro valvola N: " + String(i + 1));
      lcd.setCursor(0, 2);
      lcd.print(".....");
      // delay(100);

      if (SendAndWaitAck()) // check if acknoledgment is received or if a TIMEOUT has occured
      {
        lcd.setCursor(0, 2);
        lcd.print("Aperto");
        isOpen[i] = 1; // solo quando torna acknowledgement
        timeOpen = millis();
      }
      else
      {
        lcd.setCursor(0, 2);
        lcd.print("Fallito");
        delay(5000);
      }
    }
    else if (ReturnValues[i] == 0 && isOpen[i] != 0)
    {
      // message = "C" + String(i);

      comando = CHIUDI;
      valve = Valve(i);
      finalAddress = ValveAddress(i);

      lcd.clear();
      lcd.setCursor(1, 0);
      lcd.print("Chiudo valvola N: " + String(i + 1));
      lcd.setCursor(0, 2);
      lcd.print(".....");
      // delay(100);

      if (SendAndWaitAck())
      {
        lcd.setCursor(0, 2);
        lcd.print("Chiuso");
        isOpen[i] = 0; // solo quando torna acknowledgement
      }
      else
      {
        lcd.setCursor(0, 2);
        lcd.print("Fallito");
        delay(5000);
      }
      // aggiungere codice per aspettare risposta con timer o ripetere
    }
  }
}

int SendAndWaitAck() // bisogna aggiungere un timeout
{
  ReceivedAck = false;
  unsigned long previousMillis = 0;
  unsigned long startingTime = millis();

  while (!ReceivedAck && millis() < (30000 + startingTime))
  {
    if (millis() - previousMillis >= INTERVALLO_WAIT) // ogni 3 secondi rimanda il messaggio;
    {
      previousMillis = millis();
      SendMessage();
    }
    Serial.println("Waiting for ACK...");
    RXPacketL = LT.receiveSXBuffer(0, 1500, WAIT_RX);

    PacketRSSI = LT.readPacketRSSI();
    PacketSNR = LT.readPacketSNR();

    if (RXPacketL == 0)
    {
      Serial.println("Errore ricezione");
    }
    else
    {
      packet_Received_OK(); // its a valid packet LoRa wise, but it might not be a packet we want
    }
  }

  if (ReceivedAck)
    return 1;
  else
    return 0;
}

void SendMessage()
{

  uint8_t len;
  delay(100);
  LT.startWriteSXBuffer(0);

  LT.writeUint8(destination);  // this byte defines the packet type
  LT.writeUint8(localAddress); // destination address of the packet, the receivers address
  LT.writeUint8(comando);
  LT.writeUint8(valve);        // this byte defines the packet type
  LT.writeUint8(finalAddress); // destination address of the packet, the receivers address

  len = LT.endWriteSXBuffer(); // close the packet, get the length of data to be sent

  TXPacketL = LT.transmitSXBuffer(0, (len + 2), 5000, TXpower, WAIT_TX);

  Serial.println("sending");
}

void packet_Received_OK()
{
  // a LoRa packet has been received, which has passed the internal LoRa checks, including CRC, but it could be from
  // an unknown source, so we need to check that its actually a sensor packet we are expecting, and has valid sensor data

  uint8_t len;

  LT.startReadSXBuffer(0);

  uint8_t recipient = LT.readUint8(); // the packet type received
  uint8_t sender = LT.readUint8();    // the destination address the packet was sent to
  uint8_t response = LT.readUint8();  // the source address, where the packet came from
  uint8_t AckValve = LT.readUint8();
  uint8_t AckFinalAddress = LT.readUint8();

  len = LT.endReadSXBuffer();

  printreceptionDetails(); // print details of reception, RSSI etc
  Serial.println();

  if (recipient != localAddress)
  {
    Serial.println("This message is not for me.");
    return; // skip rest of function
  }

  Serial.println("Ufficio received from: " + String(sender));
  Serial.println("Sent to: " + String(recipient));
  Serial.println("comando: " + String(response));
  Serial.println("Per valvola N. " + String(AckValve));
  Serial.println("Con destinazione finale:  " + String(AckFinalAddress));
  Serial.println("RSSI: " + String(PacketRSSI));
  Serial.println("Snr: " + String(PacketSNR));
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

void printreceptionDetails()
{
  Serial.print(F("RSSI,"));
  Serial.print(PacketRSSI);
  Serial.print(F("dBm,SNR,"));
  Serial.print(PacketSNR);
  Serial.print(F("dB,Length,"));
  Serial.print(LT.readRXPacketL());
}
