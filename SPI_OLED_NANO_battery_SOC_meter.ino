/*********************************************************************
Project DIY SOC meter for battery - using Arduino Nano and AdaFruit LCD 0.96"
Using the module 4SPI (look back the config of the OLED 128 x 64)

Between the MOSFET IRF3205 (Gate and Ground) use 10k resistor

Also in series put 2 x 4k6 resistor to analog input A0 in A1
measuring Udiff in shunt - it prevents damaging the Arduino
while the batteries are pludged and Arduino is off. 

You can get the batteries out of the container and resume the discharge later.

Credits to Adam: 
--> YouTube Video: https://www.youtube.com/embed/qtws6VSIoYk
--> http://AdamWelch.Uk
A)
adding 3 buttons for proframing the threshold voltage;
UP/DOWN/SET. 
First selecting the battLow voltage, increase/decrease by 0.1 V. 
After pressing the SET button, select the battHigh voltage, incerase/decrease by 0.1 V.
Press SET to confirm and start the measurments.
B)
Two poneciometers for battLow and battHigh treshold voltage, from 2 ÷ 12 V and 4 ÷ 16 V. 
The tunning can be changed anytime while the circuit is taking measurments and the treshold values must be diplayed on the OLED. 

Added the slave I2C protocol to send data to Arduino mega for graphic showing the SOC in Nextion display
A4 (UNO, NANO) - 20 (MEGA)
A5 (UNO, NANO) - 21 (MEGA), also common GND
VasjArtigian 3.7.2018
*********************************************************************/

#include <SPI.h>
#include <Wire.h>     // protocol uses A4 and A5 pins!
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <EEPROM.h> 
// For software SPI:
#define OLED_MOSI   11
#define OLED_CLK   13
#define OLED_DC    9
#define OLED_CS    10
#define OLED_RESET 8
Adafruit_SSD1306 display(OLED_MOSI, OLED_CLK, OLED_DC, OLED_RESET, OLED_CS);
#define LOGO16_GLCD_HEIGHT 16 
#define LOGO16_GLCD_WIDTH  16 

#if (SSD1306_LCDHEIGHT != 64)
//#error("Height incorrect, please fix Adafruit_SSD1306.h!");
#endif
  
#define gatePin 3   // Swtiching the LED indicator for CHARGING
#define RelayCHARGE 2
#define RelayLOAD 6
#define ButtonReset 4    // reset the counter and charge value
#define ButtonSet 5     // set the voltage tresholds
#define selectMode 7  // mode 0: internal shunt and load (TEST battery mode), mode 1: external shunt (CAR mode)
#define U1 A6  // Reading Shunt Voltage1 
#define U2 A7  // Reading Shunt Voltage2 (MODE 0)
#define PotLow A2    // for setting the Voltage Treshold Low 2 ÷ 12 V
#define PotHigh A3  // for setting the voltage treshold High 4 ÷ 16 V
#define Ubattery A0    // reading the external shunt voltage1 (MODE 1) 
//#define U2ext A1    // reading the external shunt voltage2 (MODE 1)

int interval = 4000;  //Interval (ms) between measurements
float shuntRes1 = 0.56;  // In Ohms - Shunt resistor resistance
float current1 = 0.0;
float Ubat = 0.0;
float shuntU1 = 0.0;
float shuntU2 = 0.0;
bool ButtonReset_HIGH = 0;
bool ButtonSet_HIGH = 0;
bool selectMode_HIGH = 0;
float Qmax;
float Q;
float battLow = 3.2;
float battHigh = 4.2;   // default values for 18650
float voltRef = 4.63;    // Reference voltage (probe your 5V pin)
float SOC;
int SOC2;
unsigned long previousMillis = 0;
unsigned long millisPassed = 0;
int counter = 0;
// location for store data, Arduino Nano has 1 kB of EEPROM 
int eeAddress = 0;    // store the Q value
int eeAddress1 = 10;     // store the counter value
int eeAddress2 = 20;      // store the SOC value
int eeAddress3 =30;   // store the Qmax value

void setup() {
  Wire.begin();
  Serial.begin(9600);
  display.begin(SSD1306_SWITCHCAPVCC);
  // init done
  // Show image buffer on the display hardware.
  display.display();
  delay(1000);
  display.clearDisplay();
  pinMode(gatePin, OUTPUT);   
  pinMode(RelayCHARGE, OUTPUT);
  pinMode(RelayLOAD, OUTPUT);
  pinMode(ButtonReset, INPUT);
  pinMode(ButtonSet, INPUT);
  pinMode(selectMode, INPUT);
  
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0,0);  
  display.println("      Battery SOC");
  display.setCursor(0,8);
  display.println("      by elec3go");
  display.setCursor(0,16);
  display.println("set batt Low & High");
  display.setCursor(0,24);
  display.println("When done, press Set");
  display.display();
  delay(interval);
  display.clearDisplay();
  
  while (ButtonSet_HIGH == LOW) {
    ButtonSet_HIGH = digitalRead(ButtonSet);
    battLow = 2 + analogRead(PotLow)* 10/1024.0;     // from 2 ÷ 12 V
    battHigh = 3 + analogRead(PotHigh)* 13/1024.0;    // from 3 ÷ 16 V
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(0,0);  
    display.println("Low Volt. Treshold:");
    display.setCursor(30,8);
    display.println(battLow);
    display.setCursor(60,8);
    display.println("V");
    display.setCursor(0,16);
    display.println("High Volt. Treshold:");
    display.setCursor(30,24);
    display.println(battHigh);
    display.setCursor(60,24);
    display.println("V");
    display.display();
    delay(200);
    display.clearDisplay();
    }
    
// Opening the MOSFET gates for bottom balance mode
  digitalWrite(gatePin, HIGH);
  digitalWrite(RelayCHARGE, HIGH);
  digitalWrite(RelayLOAD, LOW);
  // reading the previus state from EEPROM
  Q = EEPROM.read(eeAddress);
  counter = EEPROM.read(eeAddress1);
  SOC = EEPROM.read(eeAddress2);
  Qmax = EEPROM.read(eeAddress3);
  selectMode_HIGH = digitalRead(selectMode);
}
 
void loop() {
  Wire.beginTransmission(8);
  Wire.write(SOC2);
  Wire.endTransmission();
  selectMode_HIGH = digitalRead(selectMode);
  Serial.println(selectMode_HIGH);
  Serial.println(ButtonSet_HIGH);     
  if (ButtonReset_HIGH == HIGH) {      // Clearing the EEPROM memory when button pressed
    Q = 0;
    counter = 0;
    SOC = 0;
    EEPROM.write(eeAddress, 0);
    EEPROM.write(eeAddress1, 0);
    EEPROM.write(eeAddress2, 0);
    EEPROM.write(eeAddress3, 0);
    display.clearDisplay();
    display.setTextSize(2);
    display.setTextColor(WHITE);
    display.setCursor(20,0);
    display.println("  DATA");
    display.setCursor(10,16);
    display.println(" DELETED");
    display.display();
    delay(interval);
    digitalWrite(gatePin, HIGH);
    digitalWrite(RelayCHARGE, HIGH);
    digitalWrite(RelayLOAD, LOW);
  }
  EEPROM.write(eeAddress, Q);
  EEPROM.write(eeAddress1, counter);
  EEPROM.write(eeAddress2, SOC);
  EEPROM.write(eeAddress3, Qmax);
  // for diagnostic: (on serial interface)
  Serial.print(eeAddress);
  Serial.print("\t");
  Serial.print(Q, DEC);
  Serial.println();
  Serial.print(eeAddress1);
  Serial.print("\t");
  Serial.print(counter, DEC);
  Serial.println();
  Serial.print(eeAddress2);
  Serial.print("\t");
  Serial.print(SOC, DEC);
  Serial.println();

  Ubat = analogRead(Ubattery) * voltRef /1024.0;
  shuntU1 = analogRead(U1) * voltRef / 1024.0;
  shuntU2 = analogRead(U2) * voltRef / 1024.0;
  ButtonReset_HIGH = digitalRead(ButtonReset);

// Measuring time, calculating current and charge:
  millisPassed = millis() - previousMillis;
  current1 = (shuntU1 - shuntU2) / shuntRes1;
  Q = Q + (current1 * 1000.0) * (millisPassed / 3600000.0);
  previousMillis = millis();

  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.setCursor(0,0);
  display.println("SOC:");
  display.setCursor(60,0);
  
  display.println(SOC2);
  display.setCursor(100,0);
  display.println("%");
  display.setTextSize(1);
  display.setTextColor(WHITE);   
  display.setCursor(0,16);        
  display.println(battLow);    
  display.setCursor(50,16);        
  display.println(Ubat);    // real value battery voltage
  display.setCursor(95,16);        
  display.println(battHigh);
  display.setCursor(25,16);        
  display.println("V");
  display.setCursor(75,16);        
  display.println("V");
  display.setCursor(120,16);        
  display.println("V");
  display.setCursor(0,24);
  display.println(current1);
  display.setCursor(70,24);        
  display.println(Q);
  display.setCursor(35,24);       
  display.println("A");
  display.setCursor(110,24);        
  display.println("mAh");
  display.display();
  delay(interval);
// 0, 8, 16, 24 are rows points
// WHEN 
  if(shuntU2 > shuntU1) {           //charging - LED ON
    SOC=((Qmax-Q)/Qmax)*100;
    // connect the LOAD
    if (SOC >= 100) {          // to prevent values above 100 %
      SOC = 100;
      }
    if (Ubat > battHigh) {    //battery FULL
      if (counter == 0) {      // first charge - setting bottom balance MODE
        SOC = 100;              // set to 100% when Ubatt reached the top treshold voltage
        Q = 0;}                 // and clear the charge value, set to 0.
      digitalWrite(gatePin, LOW);   // LED indication battery LOAD mode!
      digitalWrite(RelayCHARGE, LOW);     // Relay switches to LOAD mode, break the CHARGING switch (unplugge the charger)
      digitalWrite(RelayLOAD, HIGH);
      Q = 0;          // IMPRTANT - END OF charge CYCLE and start new loop, when the battery is FULL 
      counter++;      // counter of full cycles - top
      }
    }
  else if(shuntU2 < shuntU1) {             //discharging - LED OFF
    SOC=((Qmax-Q)/Qmax)*100;
    if (SOC <= 0) {                 // prevent negative values of SOC
      SOC = 0;
      }
    if (Ubat < battLow) {           //battery EMPTY
      if (counter == 0) {           // first discharge - setting top balance MODE
        SOC = 0;                    // set to value 0% when Ubatt reached the bottom treshold voltage
        Q = 0;}                     // and clear the vharge values, set to 0
      digitalWrite(gatePin, HIGH); // LED indication CHARGING mode! 
      digitalWrite(RelayCHARGE, HIGH);   // Relay break LOAD circuit, switch to CHARGE mode (or plugin the charger)
      digitalWrite(RelayLOAD, LOW);
      Qmax=Q;                     // IMPRTANT - this is the charge value for discharging proccess, value used in charging process
      //counter++;                   // counter of full cycles - bottom
      SOC = 0;
      }
    }
    SOC2 = (int)SOC;    // FOR displaying without decimal points
}

    
