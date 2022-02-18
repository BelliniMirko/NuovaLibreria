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
#include <LoRa.h>

byte UFFICIO = 0x11;

byte APRI = 0x01;
byte CHIUDI = 0x02;
byte ACKAPERTO = 0x03;
byte ACKCHIUSO = 0x04;

int isOpen = 2; // 2 means undefined, 1 is opened and 0 is closed

byte localAddress = 0xCC; // address of this device
// byte destination = 0xFF;  // destination to send to
// long lastSendTime = 0;    // last send time
// int interval = 2000;      // interval between sends

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

    Serial.println("LoRa Duplex Valvola 1");

    // override the default CS, reset, and IRQ pins (optional)

    if (!LoRa.begin(433E6))
    { // initialize ratio at 915 MHz
        Serial.println("LoRa init failed. Check your connections.");
        while (true)
            ; // if failed, do nothing
    }

    Serial.println("LoRa init succeeded.");
}

void loop()
{
    // if (millis() - lastSendTime > interval)
    // {
    //     String message = "HeLoRa World!\n"; // send a message
    //     sendMessage(message);
    //     Serial.println("Sending " + message);
    //     lastSendTime = millis();         // timestamp the message
    //     interval = random(10000) + 5000; // 2-3 seconds
    // }

    // parse for a packet, and call onReceive with the result:
    onReceive(LoRa.parsePacket());
}

void sendMessage(byte destination, byte comando, byte valve, byte finalAddress)
{
    LoRa.beginPacket();       // start packet
    LoRa.write(destination);  // add destination address
    LoRa.write(localAddress); // add sender address
    LoRa.write(comando);
    LoRa.write(valve);
    LoRa.write(finalAddress);
    LoRa.endPacket(true); // finish packet and send it

    //LoRa.dumpRegisters(Serial);
}

void onReceive(int packetSize)
{
    if (packetSize == 0)
        return; // if there's no packet, return

    // read packet header bytes:
    byte recipient = LoRa.read(); // recipient address
    byte sender = LoRa.read();    // sender address
    byte comando = LoRa.read();   // incoming msg ID
    byte valve = LoRa.read();
    byte finalAddress = LoRa.read(); // incoming msg length

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

    // if the recipient isn't this device or broadcast,
    if (recipient != localAddress && recipient != 0xFF)
    {
        Serial.println("This message is not for me.");
        return; // skip rest of function
    }

    // if message is for this device, or broadcast, print details:
    Serial.println("Received from: 0x" + String(sender, HEX));
    Serial.println("Sent to: 0x" + String(recipient, HEX));
    Serial.println("comando: " + String(comando, HEX));
    Serial.println("Per valvola N. " + String(valve, HEX));
    Serial.println("Con destinazione finale:  " + String(finalAddress, HEX));
    Serial.println("RSSI: " + String(LoRa.packetRssi()));
    Serial.println("Snr: " + String(LoRa.packetSnr()));
    Serial.println();

    if (finalAddress > localAddress)
    {
        //sendMessage(BROADCAST, comando, valve, finalAddress);
        Serial.println("NON C'È UNA QUARTA VALVOLA")
        return;
    }
    else if (finalAddress < localAddress)
    {
        //sendMessage(0xAA, comando, valve, finalAddress);
        Serial.println("NON È PER ME")
        return;
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

                sendMessage(0xBB, ACKAPERTO, valve, UFFICIO);
            }
            else if (comando == CHIUDI)
            {
                if (isOpen != 0)
                    isOpen = chiudi();
                sendMessage(0xBB, ACKCHIUSO, valve, UFFICIO);
            }
        }
        else
        {
            Serial.println("QUALCHE ERRORE NELL'ENCODING");
            return;
        }
    }
}