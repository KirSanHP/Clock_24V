/* Master program for old factory 24v clocks, using positive polarity impulse to switch even minutes
 *  and negative polarity impulse for other minutes.
 Works with real time modules DS1302, DS1307 or DS3231*/
 
// Присоединён дисплей, сделан вывод времени на дисплей
// в процессе менюшка с переводом времени

#include <EEPROM.h>
#include <iarduino_RTC.h>
//#include <LiquidCrystal.h>
#include <LiquidCrystal_I2C.h>

#define BTN_RIGHT_PIN 14   //A0
#define BTN_LEFT_PIN 15    //A1
#define BTN_SELECT_PIN 16  //A2
#define VOLT_PIN A1        //sensor pin for power voltage control

#define HOUR_ADD 0  //address in EEPROM for saving previous hour arrow position, 1 byte
#define MIN_ADD 1   //address in EEPROM for saving previous minute arrow position, 1 byte
#define MIN_CLOCK_VOLTAGE 19    //Minimum voltage required for stable clock work
#define BTN_IGNORE_TIME 80      //Time to ignore button bounce
#define MENU_WAIT_TIME 20000    //Time from last button action to return to main screen

struct Btn {
  byte pin;
  bool currentState;
  bool prevState;
  unsigned long pressMillis;
  byte time;
  bool front;
  void check() {
    currentState = !digitalRead (pin);
    unsigned long currentMillis = millis();
    front = (currentState && !prevState && (currentMillis > (millis() + BTN_IGNORE_TIME) ) );
    if (front)
      pressMillis = millis();
  };
  bool pressed() {
    return (currentState && prevState);
  }
};

enum Menu {
  MAIN,
  SET_DATE,
  SET_TIME,
  ENTER_DATE,
  ENTER_TIME,
  ENTER_ARROWS_TIME,
};

struct Disp {
  bool needRefresh;
  Menu menu;
};

//iarduino_RTC time (RTC_DS1302, 0, 1, 2);                     //RST, CLK, DAT
//iarduino_RTC time (RTC_DS1307);                              //using I2C
iarduino_RTC time (RTC_DS3231);                                //using I2C
//LiquidCrystal lcd (8, 9, 4, 5, 6, 7);                        //for display shield: RS, E, D4, D5, D6, D7, R/W = GND
LiquidCrystal_I2C lcd(0x3f, 16, 2);                            //Set address 0x3f, display 16 symbols and 2 lines (16х2)

//Menu menu = MAIN;
byte out1pin = 16;     //A2
byte out2pin = 17;     //A3
byte clock_h = 0;
byte clock_m = 0;

Btn btnRight = {BTN_RIGHT_PIN, 0, 0, 0, 0, 0};
Btn btnLeft = {BTN_LEFT_PIN, 0, 0, 0, 0, 0};
Btn btnSelect = {BTN_SELECT_PIN, 0, 0, 0, 0, 0};
Disp disp = {true, MAIN};

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
  if (EEPROM.get (HOUR_ADD, clock_h) == 255) {
    disp.menu = ENTER_ARROWS_TIME;
    clock_h = 0;
    clock_m = 0;
  }
  else {
    clock_h = EEPROM.get (HOUR_ADD, clock_h);
    clock_m = EEPROM.get (MIN_ADD, clock_m);
  }
}

void loop() {
  if (disp.needRefresh)
    dispRefresh(disp.menu);
  btnLeft.check();
  btnRight.check();
  btnSelect.check();
  
  if (millis() % 1000 == 0) {         //update time from RTC module
    time.gettime();
    if (time.hours == 12)
      time.hours = 0;
    dispPrintTime (8, 0, time.hours, time.minutes);
  }

  if (powerGood(VOLT_PIN) ) {
    if (timeNotMatch() ) {
      digitalWrite (LED_BUILTIN, LOW);
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


void dispRefresh (byte _menu) {
  switch (_menu) {
    case MAIN:
      lcd.setCursor (0, 0);
      lcd.print ("Module:");
      lcd.setCursor (0, 1);
      lcd.print ("Clock: ");
      dispPrintTime (8, 0, time.hours, time.minutes);
      dispPrintTime (8, 1, clock_h, clock_m);
      break;
    case SET_DATE:
      break;
    case SET_TIME:
      break;
    case ENTER_ARROWS_TIME:
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

void clockSwitch (byte minutes) {
  byte _pin;
  if (minutes % 2 == 0)
    _pin = out1pin;
  else
    _pin = out2pin;

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

bool powerGood(byte _pin) {
  byte voltage = (map ( (analogRead (_pin) / 4), 0, 255, 0, 24) );
  return (voltage > MIN_CLOCK_VOLTAGE);
}

void I2C_lcdStart() {
  lcd.init();                     // инициализация LCD
  lcd.backlight();                // включаем подсветку
}

void print_main_screen() {
  lcd.setCursor (0, 0);
  lcd.print ("Module");
  lcd.setCursor (0, 1);
  lcd.print ("Clock");
}

void formatTime() {   //resets minutes if minutes == 60 and resets hours if hours == 12
  if (clock_m == 60) {
    clock_m = 0;
    clock_h ++;
    if (clock_h == 12)
      clock_h = 0;
  }
}

void print_time (byte _symb, byte _line, byte _hour, byte _min) {
  lcd.setCursor (_symb, _line);
  if (_hour < 10)
    lcd.print ("0");
  lcd.print (_hour);
  lcd.print (":");
  if (_min < 10)
    lcd.print ("0");
  lcd.print (_min);
}

void swapOutputs () {
  byte _temp = out1pin;
  out1pin = out2pin;
  out2pin = _temp;
}
