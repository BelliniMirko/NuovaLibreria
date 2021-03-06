/*
  LoRa Duplex communication

  Sends a message every half second, and polls continually
  for new incoming messages. Implements a one-byte addressing scheme,
  with 0xFF as the broadcast address.

  Uses readString() from Stream class to read payload. The Stream class'
  timeout may affect other functuons, like the radio's callback. For an

  created 28 April 2017
  by Tom Igoe
*/

/*

  VALVOLA 1 -- Quella sullo stradone
  LocalAddress = 0xAA
  Can send to Ufficio = 0x11 and Valvola2 = 0xBB


  // APRI = 0x01
  // CHUDI = 0x02
  // ACK = 0x3
*/

#include <SPI.h> // include libraries
#include <SX127XLT.h>
#include "Settings.h"

uint8_t UFFICIO = 'A';
uint8_t BROADCAST = 'Q';
uint8_t APRI = 1;
uint8_t CHIUDI = 2;
uint8_t ACKAPERTO = 3;
uint8_t ACKCHIUSO = 4;

int isOpen = 2; // 2 means undefined, 1 is opened and 0 is closed

uint8_t localAddress = 'C'; // address of this device
// byte destination = 0xFF;  // destination to send to
// long lastSendTime = 0;    // last send time
// int interval = 2000;      // interval between sends

uint8_t TXPacketL;
uint8_t RXPacketL;  // length of received packet
int16_t PacketRSSI; // RSSI of received packet
int8_t PacketSNR;

SX127XLT LT;

int apri()
{
    digitalWrite(A4, HIGH);
    digitalWrite(A1, LOW);

    delay(80);

    digitalWrite(A4, LOW);

    delay(100);

    return 1;
}

int chiudi()
{
    digitalWrite(A1, HIGH);
    digitalWrite(A4, LOW);

    delay(80);

    digitalWrite(A1, LOW);

    delay(100);

    return 0;
}

void setup()
{
    pinMode(A1, OUTPUT);
    pinMode(A4, OUTPUT);

    Serial.begin(9600); // initialize serial
    while (!Serial)
        ;

    Serial.println("LoRa Duplex Valvola 2 angolo");

    // override the default CS, reset, and IRQ pins (optional)

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
}

void loop()
{
    Serial.println("I'm in loop");                                                               
    RXPacketL = LT.receiveSXBuffer(0, 0, WAIT_RX);

    PacketRSSI = LT.readPacketRSSI();
    PacketSNR = LT.readPacketSNR();

    if (RXPacketL == 0)
    {
        Serial.println("Errore invio");
    }
    else
    {
        packet_Received_OK(); // its a valid packet LoRa wise, but it might not be a packet we want
    }
}

void sendMessage(uint8_t destination, uint8_t comando, uint8_t valve, uint8_t finalAddress)
{

    delay(200);
    
    uint8_t len;

    LT.startWriteSXBuffer(0);

    LT.writeUint8(destination);  // this byte defines the packet type
    LT.writeUint8(localAddress); // destination address of the packet, the receivers address
    LT.writeUint8(comando);
    LT.writeUint8(valve);        // this byte defines the packet type
    LT.writeUint8(finalAddress); // destination address of the packet, the receivers address

    len = LT.endWriteSXBuffer(); // close the packet, get the length of data to be sent

    TXPacketL = LT.transmitSXBuffer(0, (len + 2), 5000, TXpower, WAIT_TX);
}



void packet_Received_OK()
{
    // a LoRa packet has been received, which has passed the internal LoRa checks, including CRC, but it could be from
    // an unknown source, so we need to check that its actually a sensor packet we are expecting, and has valid sensor data

    uint8_t len;

    LT.startReadSXBuffer(0);

    uint8_t recipient = LT.readUint8(); // the packet type received
    uint8_t sender = LT.readUint8();    // the destination address the packet was sent to
    uint8_t comando = LT.readUint8();   // the source address, where the packet came from
    uint8_t valve = LT.readUint8();
    uint8_t finalAddress = LT.readUint8();

    len = LT.endReadSXBuffer();

    printreceptionDetails(); // print details of reception, RSSI etc
    Serial.println();

    if (recipient != localAddress)
    {
        Serial.println("This message is not for me.");
        return; // skip rest of function
    }

    Serial.println("Received from: 0x" + String(sender));
    Serial.println("Sent to: 0x" + String(recipient));
    Serial.println("comando: " + String(comando));
    Serial.println("Per valvola N. " + String(valve));
    Serial.println("Con destinazione finale:  " + String(finalAddress));
    Serial.println("RSSI: " + String(PacketRSSI));
    Serial.println("Snr: " + String(PacketSNR));
    Serial.println();

    if (finalAddress > localAddress)
    {
        sendMessage(BROADCAST, comando, valve, finalAddress);
    }
    else if (finalAddress < localAddress)
    {
        sendMessage('B', comando, valve, finalAddress);
    }
    else
    {
        if (valve == localAddress)
        {
            Serial.println("I'm here");
            if (comando == APRI)
            {
                if (isOpen != 1)
                    isOpen = apri();

                sendMessage('B', ACKAPERTO, valve, UFFICIO);
            }
            else if (comando == CHIUDI)
            {
                if (isOpen != 0)
                    isOpen = chiudi();
                sendMessage('B', ACKCHIUSO, valve, UFFICIO);
            }
        }
        else
        {
            Serial.println("QUALCHE ERRORE NELL'ENCODING");
            return;
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
