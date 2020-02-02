/* Master program for old factory 24v clocks, using positive polarity impulse to switch even minutes
 * and negative polarity impulse for other minutes.
 * Works with real time modules DS1302, DS1307 or DS3231*/

#include <EEPROM.h>
#include <iarduino_RTC.h>
//#include <LiquidCrystal.h>
#include <LiquidCrystal_I2C.h>

#define MIN_CLOCK_VOLTAGE 19    //Minimum voltage required for stable clock work
#define BTN_IGNORE_TIME 30      //Time to ignore button bounce
#define MENU_WAIT_TIME 20000    //Time from last button action to return to main screen

#define BTN_RIGHT_PIN 14   //A0
#define BTN_LEFT_PIN 15    //A1
#define BTN_SELECT_PIN 16  //A2
#define VOLT_PIN A1        //sensor pin for 24V power voltage control

#define HOUR_ADD 0              //address in EEPROM for saving previous hour arrow position, 1 byte
#define MIN_ADD 1               //address in EEPROM for saving previous minute arrow position, 1 byte
#define SWAP_OUTPUTS_ADD 2              //Are outputs swapped or not

struct Btn {
  byte pin;
  bool isPressed;
  bool wasPressed;
  unsigned long pressMillis;    //Moment, when the button was pressed
  unsigned long currentMillis;
  bool front;
  void check() {
    isPressed = !digitalRead (pin);
    currentMillis = millis();
    if (currentMillis > (pressMillis + BTN_IGNORE_TIME) ) {
      if (isPressed && !wasPressed) {
        front = true;
        pressMillis = currentMillis;
        wasPressed = isPressed;
      }
      else if (!isPressed && wasPressed) {
        wasPressed = isPressed;
      }
    }
    else {
      front = false;
    }
  };
  bool pressed() {
    return (isPressed && wasPressed);
  };
};

enum Menu {
  MAIN,
  SET_TIME,
  ENTER_TIME,
  SET_DATE,
  ENTER_DATE,
  SET_ARROWS_POSITION,
  ENTER_ARROWS_POSITION,
  SYNCHRO_MODE,
  ADJUST_VOLTAGE_PROBE,
  WORK,
  BACK_TO_MAIN,
};

struct Disp { //
  bool needRefresh;
  Menu menu;
  Menu menuPrev;
  bool blinkEnable;
  byte blinkSymbol;
  byte blinkLine;
};

//iarduino_RTC time (RTC_DS1302, 0, 1, 2);                     //RST, CLK, DAT
//iarduino_RTC time (RTC_DS1307);                              //using I2C
iarduino_RTC time (RTC_DS3231);                                //using I2C
//LiquidCrystal lcd (8, 9, 4, 5, 6, 7);                        //for display shield: RS, E, D4, D5, D6, D7, R/W = GND
LiquidCrystal_I2C lcd(0x3f, 16, 2);                            //Set address 0x3f, display 16 symbols and 2 lines (16х2)

bool work = true;
bool synchronizeNow = true;
byte out1pin = 16;     //A2
byte out2pin = 17;     //A3
byte clock_h = 0;
byte clock_m = 0;
bool swap_outputs = false;

Btn btnRight = {BTN_RIGHT_PIN};
Btn btnLeft = {BTN_LEFT_PIN};
Btn btnSelect = {BTN_SELECT_PIN};
Disp disp = {true};

void setup() {
  time.begin();
  time.gettime();
  if (time.hours == 12)
    time.hours = 0;

  I2C_lcdStart();

  pinMode (out1pin, OUTPUT);
  pinMode (out2pin, OUTPUT);
  pinMode (BTN_RIGHT_PIN, INPUT_PULLUP);
  pinMode (BTN_LEFT_PIN, INPUT_PULLUP);
  pinMode (BTN_SELECT_PIN, INPUT_PULLUP);
  pinMode (VOLT_PIN, INPUT);
  pinMode (LED_BUILTIN, OUTPUT);
  if (EEPROM.get (HOUR_ADD, clock_h) == 255) {  // menu = enter arrows position
    disp.menu = ENTER_ARROWS_POSITION;
    work = false;
//-------- move this to "Enter arrows position"
    swap_outputs = false;
    EEPROM.put (SWAP_OUTPUTS_ADD, swap_outputs);
    
    clock_h = 0;
    clock_m = 0;
  }
  else {
    clock_h = EEPROM.get (HOUR_ADD, clock_h);
    clock_m = EEPROM.get (MIN_ADD, clock_m);
    swap_outputs = EEPROM.get (SWAP_OUTPUTS_ADD, swap_outputs);
    if (swap_outputs)
      swapOutputs();
  }
  btnRight.check();
  btnLeft.check();
  if (btnRight.isPressed && btnLeft.isPressed)
    work = false;
}

void loop() {
  btnLeft.check();
  btnRight.check();
  btnSelect.check();

  if (btnLeft.front || btnRight.front || btnSelect.front) {
    
  }

  //----------add button actions
  if (disp.needRefresh) {
    dispRefresh();
  }
  
  if (millis() % 1000 == 0) {         //update time from RTC module
    time.gettime();
    if (time.hours == 12)
      time.hours = 0;
    if (disp.menu == MAIN) {
      dispPrintTime (8, 0, time.hours, time.minutes);
      dispPrintSeconds (13, 0, time.seconds);
    }
  }

  if (powerGood() && work) {   //turn off LED
      digitalWrite (LED_BUILTIN, LOW);
    if (timeNotMatch() ) {   // disp.refresh = true, clock.m++, formatTime, EEPROM.put, clock.switch
      if (disp.menu == MAIN)
        dispPrintTime (8, 0, time.hours, time.minutes);
      clock_m ++;
      formatTime();
      
      EEPROM.put (MIN_ADD, clock_m);
      EEPROM.put (HOUR_ADD, clock_h);
      clockSwitch (clock_m);    //turn on out > delay > turn off out > delay
    }
  }
  else
    digitalWrite (LED_BUILTIN, HIGH);
}
//---------end of loop()


void dispRefresh () {
  lcd.clear();
  lcd.setCursor (0, 0);
  lcd.noBlink();
  switch (disp.menu) {
    case MAIN: {
      lcd.print ("Module:");
      lcd.setCursor (0, 1);
      lcd.print ("Clock: ");
      dispPrintTime (8, 0, time.hours, time.minutes);
      lcd.print (":");
      dispPrintSeconds (14, 0, time.seconds);
      dispPrintTime (7, 1, clock_h, clock_m);
      lcd.setCursor (13, 1);
      if (work)
        lcd.print ("On ");
      else
        lcd.print ("Off");
    }
    break;
    case SET_TIME: {
      lcd.print ("Set time");
    }
    break;
    case SET_DATE: {
      lcd.print ("Set date");
    }
    break;
    case SET_ARROWS_POSITION: {
      lcd.print ("Set");
      lcd.setCursor (0, 1);
      lcd.print ("arrows position");
    }
    break;
  }
  disp.needRefresh = false;
}

void dispPrintTime (byte _symbol, byte _line, byte _hour, byte _minute) {
  lcd.setCursor (_symbol, _line);
  if (_hour < 10)
    lcd.print ("0");
  lcd.print (_hour);
  lcd.print (":");
  if (_minute < 10)
    lcd.print ("0");
  lcd.print (_minute);
}

void dispPrintSeconds (byte _symbol, byte _line, byte _seconds) {
  lcd.setCursor (_symbol, _line);
  if (_seconds < 10)
    lcd.print ("0");
  lcd.print (_seconds);
}

void clockSwitch (byte minutes) {   //add display output of current position at avery switch
  byte _pin;
  if (minutes % 2 == 0)
    _pin = out1pin;
  else
    _pin = out2pin;
  //--------add display output here
  digitalWrite (_pin, HIGH);
  digitalWrite (LED_BUILTIN, HIGH);
  delay (100);
  digitalWrite (_pin, LOW);
  digitalWrite (LED_BUILTIN, LOW);
  delay (100);
}

bool timeNotMatch() {
  return (time.hours != clock_h || time.minutes != clock_m);
}

bool powerGood() {
  byte voltage = (map ( (analogRead (VOLT_PIN) / 4), 0, 255, 0, 24) );
  return (voltage > MIN_CLOCK_VOLTAGE);
}

void I2C_lcdStart() {
  lcd.init();                     // инициализация LCD
  lcd.backlight();                // включаем подсветку
}

void formatTime() {   //resets clock minutes if minutes == 60 and resets clock hours if hours == 12
  if (clock_m == 60) {
    clock_m = 0;
    clock_h ++;
    if (clock_h == 12)
      clock_h = 0;
  }
}

void swapOutputs () {
  byte _temp = out1pin;
  out1pin = out2pin;
  out2pin = _temp;
}
