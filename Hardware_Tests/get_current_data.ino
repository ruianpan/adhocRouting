/*
 * @brief Arduino microcontroller loop, which reads the power consumption 
	  from the curent sensor, and outputs it to serial immediately.
	  Wiring:
	  * analog pin A0 is connected to GPIO_DATA, and deliminates data collection periods
	  * analog pin A1 is connected to GPIO_DONE, and signals the end of transmission
*/

#include <Adafruit_INA260.h>

Adafruit_INA260 ina260 = Adafruit_INA260();

#define MAX_CT 1

int ledPin= 3;
int RPI_ready= A0;
int RPI_done= A1;

void setup() {
  Serial.begin(115200);
  // Wait until serial port is opened
  while (!Serial) { delay(10); }

  if (!ina260.begin()) {
    Serial.println("Couldn't find INA260 chip");
    while (1);
  }
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW);
  Serial.println("Sample Time (ms), Average Power (mW)");
  
}

int analogVal=0;
int procDone=0;

unsigned long currTime;
int curr_data;
int getTimeOnce=1;

void loop() {
  digitalWrite(ledPin, LOW);
  analogVal= analogRead(RPI_ready);
  
  procDone= analogRead(RPI_done);
  
  if(procDone > 20){
    Serial.println("0,0");
    delay(1000);
  }
  
  //must have received a signal from the RPi to begin data collection
  if(analogVal > 20){
    if(getTimeOnce == 1){
      currTime= millis();
      getTimeOnce= 0;
    }
    digitalWrite(ledPin, HIGH);
    curr_data= ina260.readPower();
    Serial.print(millis() - currTime);
    Serial.print(",");
    Serial.println(curr_data);
  }
}
