/*Notes: 
http://jeelabs.org/2012/05/04/measuring-vcc-via-the-bandgap/
Used RoomNode as example
William Garrido
http://mobilewill.us
CC-BY-SA
*/
#include <JeeLib.h>
#include <avr/sleep.h>
#include <util/atomic.h>

#define SERIAL  0   // set to 1 to also report readings on the serial port
#define DEBUG   0   // set to 1 to display each loop() run and RSW trigger
#define DEBUG_LED 0 // Use LED to help with debugging measurements and transmissions

//Pick one
#define THERM 1
#define TMP 0
#define DHT11 0
#define DALLAS 0

#define LDR_PIN A6 
#define TMP_PIN A7
#define TMP36_PIN A3
#define LED_PIN 9
#define RSW_PIN 3
#define DHT11_PIN 5

#define RSW_DEBOUNCE 9000

#define MEASURE_PERIOD  600 // how often to measure, in tenths of seconds
#define RETRY_PERIOD    10  // how soon to retry if ACK didn't come in
#define RETRY_LIMIT     5   // maximum number of times to retry
#define ACK_TIME        10  // number of milliseconds to wait for an ack
#define REPORT_EVERY    5   // report every N measurement cycles
#define SMOOTH          3   // smoothing factor used for running averages

// set the sync mode to 2 if the fuses are still the Arduino default
// mode 3 (full powerdown) can only be used with 258 CK startup fuses
#define RADIO_SYNC_MODE 2

#if DHT11
  #include <dht.h>
  dht DHT;
#endif

// The scheduler makes it easy to perform various tasks at various times:

enum { MEASURE, REPORT, TASK_END };

static word schedbuf[TASK_END];
Scheduler scheduler (schedbuf, TASK_END);

// Other variables used in various places in the code:

static byte reportCount;    // count up until next report, i.e. packet send
static byte myNodeID;       // node ID used for this unit

//volatile bool adcDone;
//ISR(ADC_vect) { adcDone = true; }

const int NUMSAMPLES = 5;
   
struct {
      byte light;
      byte reed :1;
      byte humi :7;
      byte temp :10;
      byte lobat :1;
      //byte moved :1;
      //int battery :13;
}payload;
  
  
#if RSW_PIN
    #define RSW_PULLUP      1   // set to one to pull-up the PIR input pin
    #define RSW_INVERTED    1   // 0 or 1, to match PIR reporting high or low
    
    /// Interface to a reed switch.
    class RSW{volatile byte value, changed; volatile uint32_t lastChanged, diff, preChanged;
    public:
        RSW () : value (0), changed (0), lastChanged (millis()), diff (0), preChanged(0) {}

        // this code is called from the interrupt handler
        void poll() {
            // see http://talk.jeelabs.net/topic/811#post-4734 for PIR_INVERTED
            //Requires cap on input to filter "bounce" in the sensor from closing window (ie not closing window directly in once try. 100nf works well. 
            digitalWrite(LED_PIN, HIGH);
            value = digitalRead(RSW_PIN); //^ RSW_INVERTED;
            // if the pin just went on, then set the changed flag to report it
            //if (pin) {
             changed = 1;
              //value = pin;
             digitalWrite(LED_PIN, LOW);
         }
        
        // state is true if curr value is still on
        byte state() const {
            byte f = value;
            return f;
        }

        // return true if there is new motion to report
        byte triggered() {
            byte f = changed;
            changed = 0;
            return f;
        }

    };

    RSW rsw;
    
   void callISR(){rsw.poll();}
   //ISR(PCINT2_vect) { rsw.poll(); }


#endif


// has to be defined because we're using the watchdog for low-power waiting
ISR(WDT_vect) { Sleepy::watchdogEvent(); }


/*
static byte readVcc(byte count =4) {
  set_sleep_mode(SLEEP_MODE_ADC);
  ADMUX = bit(REFS0) | 14; // use VCC and internal bandgap
  bitSet(ADCSRA, ADIE);
  while (count-- > 0) {
    adcDone = false;
    while (!adcDone)
      sleep_mode();
  }
  bitClear(ADCSRA, ADIE);  
  // convert ADC readings to fit in one byte, i.e. 20 mV steps:
  //  1.0V = 0, 1.8V = 40, 3.3V = 115, 5.0V = 200, 6.0V = 250
  return (55U * 1023U) / (ADC + 1) - 50;
}
*/

static int readVcc (byte us =250){
  analogRead(6);
  bitSet(ADMUX, 3);
  delayMicroseconds(us);
  bitSet(ADCSRA, ADSC);
  while (bit_is_set(ADCSRA, ADSC));
  word x = ADC;
  return x ? (1100L * 1023) / x : -1; 
}

// utility code to perform simple smoothing as a running average
static int smoothedAverage(int prev, int next, byte firstTime =0) {
    if (firstTime)
        return next;
    return ((SMOOTH - 1) * prev + next + SMOOTH / 2) / SMOOTH;
}

// wait a few milliseconds for proper ACK to me, return true if indeed received
static byte waitForAck() {
    MilliTimer ackTimer;
    while (!ackTimer.poll(ACK_TIME)) {
        if (rf12_recvDone() && rf12_crc == 0 &&
                // see http://talk.jeelabs.net/topic/811#post-4712
                rf12_hdr == (RF12_HDR_DST | RF12_HDR_CTL | myNodeID))
            return 1;
        set_sleep_mode(SLEEP_MODE_IDLE);
        sleep_mode();
    }
    return 0;
}

float toF(float degreesC){
  return 1.8 * degreesC + 32.0;
}

double getTemp()
{
  #if THERM
    int RawTemp = 0;
    for (int i = 0; i < NUMSAMPLES; i++)
    {
      RawTemp += analogRead(TMP_PIN);
    }
    RawTemp /= NUMSAMPLES;
    long Resistance;	
    double Temp;
    Resistance=((10240000/RawTemp) - 10000); 
    Temp = log(Resistance); // Saving the Log(resistance) so not to calculate  it 4 times later
    Temp = 1 / (0.001129148 + (0.000234125 * Temp) + (0.0000000876741 * Temp * Temp * Temp)); 
    Temp = Temp - 273.15;  // Convert Kelvin to Celsius                      
    //Temp = (Temp * -1);
    Temp = toF(Temp); // Convert Celcius to Fahrenheit
      //Temp = (Temp * 9.0)/ 5.0 + 32.0; // Convert Celcius to Fahrenheit
    return Temp;
  #endif
  
  #if TMP
    float voltage, degreesC;
    voltage = analogRead(TMP36_PIN) / 310.3;
    degreesC = (voltage - 0.5) * 100.0;
    return toF(degreesC);
  #endif
  
  #if DHT11
    return toF(DHT.temperature);
  #endif
}


void sensors(){
  byte firstTime = payload.humi == 0; // special case to init running avg
  payload.lobat = rf12_lowbat();
  int readLDR = analogRead(LDR_PIN);
  byte light = 255 - readLDR / 4;
  light = map(light, 0, 255, 255, 0);
  payload.light = smoothedAverage(payload.light, light, firstTime);
  
  //payload.battery = readVcc();
  //digitalWrite(RSW_PIN, HIGH);
  payload.reed = digitalRead(RSW_PIN);
  //digitalWrite(RSW_PIN, LOW); //Save Power on pull-up
  #if DHT11
    int humi;
    int chk = DHT.read11(DHT11_PIN);
    if(chk == DHTLIB_OK){
      humi = DHT.humidity;
      payload.humi = smoothedAverage(payload.humi, humi, firstTime);  
    }
    else{
     payload.humi = 0; 
    }
    
  #else
    payload.humi = 0;
  #endif
  //payload.moved = 0;
  
  int temp = getTemp();
  payload.temp = smoothedAverage(payload.temp, temp, firstTime);
  
}

static void serialFlush () {
    Serial.flush();
    delay(2); // make sure tx buf is empty before going back to sleep
}

void sendData(){
  // actual packet send: broadcast to all, current counter, 1 byte long
  rf12_sleep(RF12_WAKEUP);
  rf12_sendNow(0, &payload, sizeof payload);
  rf12_sendWait(RADIO_SYNC_MODE);
  rf12_sleep(RF12_SLEEP);
  #if SERIAL
    Serial.print("SHA_Node:");
    Serial.print(rf12_hdr);
     Serial.print(":");
    Serial.print((int) payload.light);
    Serial.print(":");
    Serial.print((int) payload.reed);
    Serial.print(":");
    Serial.print((int) payload.humi);
    Serial.print(":");
    Serial.print((int) payload.temp);
    //Serial.print(":");
    //Serial.print((int) payload.moved);
    Serial.print(":");
    Serial.println((int) payload.lobat);
    //Serial.print(":");
    // Serial.println((int) payload.battery);
    serialFlush();
 #endif
}

// send packet and wait for ack when there is a reed trigger
static void doTrigger() {
    #if DEBUG
        Serial.print("RSW ");
        Serial.print((int) payload.reed);
        serialFlush();
    #endif
  
    sensors();

    for (byte i = 0; i < RETRY_LIMIT; ++i) {
        rf12_sleep(RF12_WAKEUP);
        rf12_sendNow(RF12_HDR_ACK, &payload, sizeof payload);
        rf12_sendWait(RADIO_SYNC_MODE);
        byte acked = waitForAck();
        rf12_sleep(RF12_SLEEP);

        if (acked) {
            #if DEBUG
                Serial.print(" ack ");
                Serial.println((int) i);
                serialFlush();
            #endif
            // reset scheduling to start a fresh measurement cycle
            scheduler.timer(MEASURE, MEASURE_PERIOD);
            return;
        }
        
        delay(RETRY_PERIOD * 100);
    }
    scheduler.timer(MEASURE, MEASURE_PERIOD);
    #if DEBUG
        Serial.println(" no ack!");
        serialFlush();
    #endif
}


void blink (byte pin=LED_PIN) {
  #if DEBUG_LED
    /*for (byte i = 0; i < 6; ++i) {
        delay(100);
        digitalWrite(pin, !digitalRead(pin));
    }*/
    digitalWrite(pin, !digitalRead(pin));
  #endif
}


void setup () {
  // need to enable the pull-up to get a voltage drop over the LDR
  //pinMode(14+LDR, INPUT_PULLUP);
  #if SERIAL || DEBUG
    Serial.begin(57600);
    Serial.print("\n[roomNode.3]");
    myNodeID = rf12_config();
    serialFlush();
  #endif
  //rf12_initialize(2, RF12_433MHZ, 100);
  //rf12_config(0);
  myNodeID = rf12_config(0);
  rf12_sleep(RF12_SLEEP); 
  pinMode(LED_PIN, OUTPUT);
  pinMode(RSW_PIN, INPUT_PULLUP);
  
  
  #if RSW_PIN
     // the RSW signal comes in via a interrupt
    attachInterrupt(1, callISR, CHANGE);
    //pciSetup(RSW_PIN);
  #endif
  
  reportCount = REPORT_EVERY;     // report right away for easy debugging
  scheduler.timer(MEASURE, 0);    // start the measurement loop going
}

void loop () {
  
  #if RSW_PIN
        if (rsw.triggered()) {
            blink();
            payload.reed = rsw.state();
            doTrigger();
            blink();
        }
  #endif
  
  switch (scheduler.pollWaiting()) {

        case MEASURE:
            blink();
            // reschedule these measurements periodically
            scheduler.timer(MEASURE, MEASURE_PERIOD);
    
            //doMeasure();
            sensors();

            // every so often, a report needs to be sent out
            if (++reportCount >= REPORT_EVERY) {
                reportCount = 0;
                scheduler.timer(REPORT, 0);
            }
            blink();
            break;
            
            
        case REPORT:
            //doReport();
            blink();
            sendData();
            blink();
            break;
    }
  
  //Sleepy::loseSomeTime(10000);
}


int freeRam () {
  extern int __heap_start, *__brkval;
  int v;
  return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval);
}


void pciSetup(byte pin)
{
    *digitalPinToPCMSK(pin) |= bit (digitalPinToPCMSKbit(pin));  // enable pin
    PCIFR  |= bit (digitalPinToPCICRbit(pin)); // clear any outstanding interrupt
    PCICR  |= bit (digitalPinToPCICRbit(pin)); // enable interrupt for the group
}
