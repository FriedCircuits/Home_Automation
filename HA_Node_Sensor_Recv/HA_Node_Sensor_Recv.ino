#include <JeeLib.h>

#define LED_PIN 9
#define NODE_ID 31

/*
typedef struct {
    byte light;
    byte moved :1;
    byte humi :7;
    int temp :10;
    byte lobat :1;
    byte reed :1;
  //int battery :13;
}Payload;
Payload measurement;
*/

static void printOneChar (char c) {
  Serial.print(c);
}

static void showString (PGM_P s) {
  for (;;) {
    char c = pgm_read_byte(s++);
    if (c == 0)
      break;
    if (c == '\n')
      printOneChar('\r');
    printOneChar(c);
  }
}

static void showByte (byte value) {
  Serial.print((word) value);
}

void setup(){
  rf12_initialize(NODE_ID, RF12_433MHZ, 100);
  Serial.begin(57600);
  pinMode(LED_PIN, OUTPUT);
}

void loop(){
  
  if (rf12_recvDone() && rf12_crc == 0){
     //&& rf12_len == sizeof (Payload)
     //measurement = *(Payload*) rf12_data;
     digitalWrite(LED_PIN, HIGH);
     byte n = rf12_len;

      showString(PSTR("OK"));
      printOneChar(' ');
      showByte(rf12_hdr);
      for (byte i = 0; i < n; ++i) {
  
              printOneChar(' ');
          showByte(rf12_data[i]);
      }
      Serial.println();
      
      if (RF12_WANTS_ACK) {
        showString(PSTR(" -> ack\n"));
        rf12_sendStart(RF12_ACK_REPLY, 0, 0);
      }
      
      digitalWrite(LED_PIN, LOW);
  }
 
}
