//Written by Dillon Shannon and Momo Alverson
//CPE 301 Fall 2023 Final Project

//initialize stepper motor library
#include <Stepper.h>
#include <LiquidCrystal.h>
#include <dht.h>
#include <RTClib.h>

#define RDA 0x80
#define TBE 0x20  

//GLOBALS/PREPROCCESSOR DIRECTIVES WRITTEN BY MOMO ALVERSON

//Serial Pointers
volatile unsigned char *myUCSR0A = (unsigned char *)0x00C0;
volatile unsigned char *myUCSR0B = (unsigned char *)0x00C1;
volatile unsigned char *myUCSR0C = (unsigned char *)0x00C2;
volatile unsigned int  *myUBRR0  = (unsigned int *) 0x00C4;
volatile unsigned char *myUDR0   = (unsigned char *)0x00C6;

//Register Pointers
//Register A pointers for LED's
volatile unsigned char *myDDRA = (unsigned char*) 0x21; //Control Register
volatile unsigned char *myPORTA = (unsigned char*) 0x22; //Write Register

//Regiser G pointers for fan motor
volatile unsigned char *myDDRG = (unsigned char*) 0x33;
volatile unsigned char *myPORTG = (unsigned char*) 0x34;

//ADC pointers for water level detector
volatile unsigned char* my_ADMUX = (unsigned char*) 0x7C;
volatile unsigned char* my_ADCSRB = (unsigned char*) 0x7B;
volatile unsigned char* my_ADCSRA = (unsigned char*) 0x7A;
volatile unsigned int* my_ADC_DATA = (unsigned int*) 0x78;


//Global Variables
enum state{error1, disabled1, idle1, running1};
volatile enum state status;
volatile bool fanOn = false;

//Temp/Humidity Sensor Object
dht DHT;

//Real Time clock
RTC_DS1307 rtc;
DateTime now;

//For 1 min humidity/temp reporting delay
unsigned long currMillis;
unsigned long prevMillis=0;
const long interval = 60000;
bool firstrun = true;

//Water sensor
unsigned int wLevel;
const unsigned int waterThresh = 260;

//For LCD
const int RS = 46, EN =44, D4 = 49, D5 = 47, D6 = 45, D7 = 43;
LiquidCrystal lcd(RS, EN, D4, D5, D6, D7);

//Stepper Motor
int ventPos = 0, newVentPos = 0;
const int stepsPerRev = 2038; //stepper motor revolution variable
Stepper myStepper = Stepper(stepsPerRev,8,10,9,11); //Stepper motor object

//INTERRUPTS WRITTEN BY MOMO ALVERSON
void ISR5(){
  if(status == disabled1){
    status = idle1;
    }
}

//Stop button
void ISR0(){
  status = disabled1;
}

//Reset Button
void ISR4(){
  if(status != error1){
    return;
  }
  wLevel = adc_read(0);
  if(wLevel >= waterThresh){
    status = idle1;
  }
}

void ISR1(){
  newVentPos++;
  newVentPos = newVentPos % 5;
}



//SetUP Written by Dillon Shannon
void setup() {
  status = disabled1; //Start the cooler in the disabled state

  *myDDRA &= 0xF0; //LED Pins set to Output
  *myDDRG &= 0xFC; //Fan motor pin 40/41 set to output
  
  //LCD Setup
  lcd.noAutoscroll();
  lcd.noBlink();
  lcd.begin(16, 2);

  //ADC Initialization
  adc_init();

  //RTC Setup
  rtc.begin();
  rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));

  //Interrupts
  attachInterrupt(5, ISR5, RISING); //Start button
  attachInterrupt(0, ISR0, RISING); //Stop button
  attachInterrupt(4, ISR4, RISING); //Reset button
  attachInterrupt(1, ISR1, RISING); //Vent/Stepper motor button

  U0init(9600);
}

//Loop Written by Dillon Shannon w/ contributions and debugging by MOMO ALVERSON
void loop() {
  delay(1000); //This delay fixed some issues with ISR's, unsure of why
  currMillis = millis();
  if(firstrun){// This will allow the LCD to start displaying temp/humidity at start up, instead of waiting a minute
    currMillis = currMillis + 60000;
    firstrun = false;
  }

  //Processes for when in not Disabled
  if(status != disabled1){
    int chk = DHT.read11(33); //Read temp/humidity
    if(currMillis - prevMillis >= interval){
      prevMillis = currMillis;
      lcd.setCursor(0,0);
      lcd.print("Temp: ");
      lcd.print(DHT.temperature);
      lcd.print((char)223);
      lcd.print("C");
      lcd.setCursor(0,1);
      lcd.print("Humidity: ");
      lcd.print(DHT.humidity);
      lcd.print("%");
    }
    vent(); //Check stepper motor
  }

  //Excute branch depending on cooler status
  if(status == error1){
    error();
  }
  else if(status == disabled1){
    disabled();
  }
  else if(status == idle1){
    idle();
  }
  else if(status == running1){
    running();
  }
}

//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
//Status Functions
//Written by Dillon Shannon
void error(){
  U0putchar('E'); U0putchar('R'); U0putchar('R'); U0putchar('O'); U0putchar('R'); U0putchar('\n');

  //Turn off all LED's
    *myPORTA &= 0xF0;
    //Turn on red LED
    *myPORTA |= 0x08;
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Error");
  lcd.setCursor(0, 1);
  lcd.print("Refill Water");

  //Turn off fan
  if(fanOn){
    U0putchar('F');U0putchar('A');U0putchar('N');U0putchar(' ');U0putchar('O');U0putchar('F');U0putchar('F');U0putchar('\n');
    outputTime();
    fanOn = false;
    *myPORTG &= 0xFE;
  }
}
//Written by Dillon Shannon
void disabled(){
  lcd.clear();
  U0putchar('D'); U0putchar('I'); U0putchar('S');U0putchar('A');U0putchar('B');U0putchar('L');U0putchar('E');U0putchar('D'); U0putchar('\n');
  //Turn off all LED's
  *myPORTA &= 0xF0;
  //Turn on yellow LED
  *myPORTA |= 0x02;

  //Turn off fan
  if(fanOn){
    U0putchar('F');U0putchar('A');U0putchar('N');U0putchar(' ');U0putchar('O');U0putchar('F');U0putchar('F');U0putchar('\n');
    outputTime();
    fanOn = false;
    *myPORTG &= 0xFE;
  }

}
//WRITTEN BY MOMO ALVERSON
void idle(){
  U0putchar('I'); U0putchar('D'); U0putchar('L'); U0putchar('E'); U0putchar('\n');
  //Turn off all LED's
    *myPORTA &= 0xF0;
    //Turn on Green LED
    *myPORTA |= 0x01;
  wLevel = adc_read(0);
  if(wLevel < waterThresh){
    status = error1;
    return;
  }
  int chk = DHT.read11(33); //Read temp/humidity
  if(DHT.temperature > 10){
    status = running1;
    return;
  }

  //Turn off fan
  if(fanOn){
    U0putchar('F');U0putchar('A');U0putchar('N');U0putchar(' ');U0putchar('O');U0putchar('F');U0putchar('F');U0putchar('\n');
    outputTime();
    fanOn = false;
    *myPORTG &= 0xFE;
  }
}
//WRITTEN BY MOMO ALVERSON
void running(){
  U0putchar('R'); U0putchar('U'); U0putchar('N');U0putchar('\n');
  //Turn off all LED's
    *myPORTA &= 0xF0;
    //Turn on Blue LED
    *myPORTA |= 0x04;
  wLevel = adc_read(0);
  if(wLevel < waterThresh){
    status = error1;
    return;
  }
  int chk = DHT.read11(33); //Read temp/humidity
  if(DHT.temperature <= 10){
    status = idle1;
    return;
  }

  //Turn on fan
  if(!fanOn){
    U0putchar('F');U0putchar('A');U0putchar('N');U0putchar(' ');U0putchar('O');U0putchar('N');U0putchar('\n');
    outputTime();
    fanOn = true;
    *myPORTG |= 0x01;
  }
}

//vent position sensor
//Written by MOMO ALVERSON
void vent(){
  while(ventPos != newVentPos){
    if(newVentPos > ventPos){
      myStepper.setSpeed(10);
      myStepper.step(stepsPerRev);
      ventPos++;
      U0putchar('V');U0putchar('E');U0putchar('N');U0putchar('T');U0putchar(' ');U0putchar('U');U0putchar('P');U0putchar('\n');
    }
    else if(newVentPos < ventPos){
      myStepper.setSpeed(10);
      myStepper.step(0-stepsPerRev);
      ventPos--;
      U0putchar('V');U0putchar('E');U0putchar('N');U0putchar('T');U0putchar(' ');U0putchar('D');U0putchar('O');U0putchar('W');U0putchar('N');U0putchar('\n');
    }
  }
}


//ADC Functions
//FROM PREVIOUS LAB
void adc_init(){
  // setup the A register
  *my_ADCSRA |= 0b10000000; // set bit   7 to 1 to enable the ADC
  *my_ADCSRA &= 0b11011111; // clear bit 6 to 0 to disable the ADC trigger mode
  *my_ADCSRA &= 0b11110111; // clear bit 5 to 0 to disable the ADC interrupt
  *my_ADCSRA &= 0b11111000; // clear bit 0-2 to 0 to set prescaler selection to slow reading
  // setup the B register
  *my_ADCSRB &= 0b11110111; // clear bit 3 to 0 to reset the channel and gain bits
  *my_ADCSRB &= 0b11111000; // clear bit 2-0 to 0 to set free running mode
  // setup the MUX Register
  *my_ADMUX  &= 0b01111111; // clear bit 7 to 0 for AVCC analog reference
  *my_ADMUX  |= 0b01000000; // set bit   6 to 1 for AVCC analog reference
  *my_ADMUX  &= 0b11011111; // clear bit 5 to 0 for right adjust result
  *my_ADMUX  &= 0b11100000; // clear bit 4-0 to 0 to reset the channel and gain bits
}

//FROM PREVIOUS LAB
unsigned int adc_read(unsigned char adc_channel_num){
  // clear the channel selection bits (MUX 4:0)
  *my_ADMUX  &= 0b11100000;
  // clear the channel selection bits (MUX 5)
  *my_ADCSRB &= 0b11110111;
  // set the channel number
  if(adc_channel_num > 7)
  {
    // set the channel selection bits, but remove the most significant bit (bit 3)
    adc_channel_num -= 8;
    // set MUX bit 5
    *my_ADCSRB |= 0b00001000;
  }
  // set the channel selection bits
  *my_ADMUX  += adc_channel_num;
  // set bit 6 of ADCSRA to 1 to start a conversion
  *my_ADCSRA |= 0x40;
  // wait for the conversion to complete
  while((*my_ADCSRA & 0x40) != 0);
  // return the result in the ADC data register
  return *my_ADC_DATA;
}

//FROM PREVIOUS LAB
//Serial communication 
void U0init(int U0baud)
{
 unsigned long FCPU = 16000000;
 unsigned int tbaud;
 tbaud = (FCPU / 16 / U0baud - 1);
 // Same as (FCPU / (16 * U0baud)) - 1;
 *myUCSR0A = 0x20;
 *myUCSR0B = 0x18;
 *myUCSR0C = 0x06;
 *myUBRR0  = tbaud;
}

void U0putchar(unsigned char U0pdata)
{
  while((*myUCSR0A & TBE)==0);
  *myUDR0 = U0pdata;
}


//WRITTEN BY DILLON SHANNON
void outputTime(){
  rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  now = rtc.now();
  U0putchar(now.hour()/10 + 48);
  U0putchar(now.hour() % 10 + 48);
  U0putchar(':');
  U0putchar(now.minute()/10 + 48);
  U0putchar(now.minute() % 10 + 48);
  U0putchar(' ');
  U0putchar(now.month() / 10 + 48);
  U0putchar(now.month() % 10 + 48);
  U0putchar('/');
  U0putchar(now.day() / 10 + 48);
  U0putchar(now.day() % 10 + 48);
  U0putchar('/');
  U0putchar((now.year() / 1000) % 10 + 48);
  U0putchar(now.year() % 10 +48);
  U0putchar('\n');
}