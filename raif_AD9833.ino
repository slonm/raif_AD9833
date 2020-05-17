#define DEBUG 0
//Назначение пинов контроллера
//LCD
const byte RS=6, E=7, DB4=5, DB5=4, DB6=3, DB7=2;
//Keypad
const byte ROWS = 4; 
const byte COLS = 3; 
const char keys[ROWS][COLS] = {
  {'1','2','3'},
  {'4','5','6'},
  {'7','8','9'},
  {'*','0','#'}
};
byte rowPins[ROWS] = {14, 15, 16, 17}; 
byte colPins[COLS] = {18, 19, 8}; 
//SPI SD card
const byte sdChipSelect = 9;
//SPI AD9833
const byte genChipSelect = SS;
//Выход "Включено"
const byte pinProgStarted = 1;
//Выход "Сигнал"
const byte pinBuzz = 0;

//https://tsibrov.blogspot.com/2018/06/ad9833.html
#include <SPI.h>
// подключаем библиотеку LiquidCrystal
#include <LiquidCrystal.h>
// библиотека клавиатуры
#include <Keypad.h>
// библиотека поддержки SD карты
#include <SD.h>

#if DEBUG
  #define SERIAL_BEGIN(a) Serial.begin(a);  while (!Serial);
  #define WRITE(a) Serial.write(a)
  #define PRINTLN(a) Serial.println(a)
  #define PRINT(a, b) Serial.print(a,b)
#else
  #define SERIAL_BEGIN(a)
  #define WRITE(a)
  #define PRINTLN(a)
  #define PRINT(a, b)
#endif
// ********** AD9833 **********
#define bMode 0x2
#define bDiv2 0x8
#define bOpbiten 0x20
#define bSleep12 0x40
#define bSleep1 0x80
#define bReset 0x100
#define bHLB 0x1000
#define bB28 0x2000
#define bCntrl_reg 0x0
#define bFreq_reg0 0x4000
#define bFreq_reg1 0x8000
#define bPhase_reg 0xC000

// инициализируем объект-экран, передаём использованные 
// для подключения контакты на Arduino в порядке:
// RS, E, DB4, DB5, DB6, DB7
LiquidCrystal lcd(RS, E, DB4, DB5, DB6, DB7);
//Инициализация объекта-клавиатуры
Keypad keypad = Keypad( makeKeymap(keys), rowPins, colPins, ROWS, COLS );

float programFrequency;
float programDelay;
uint16_t programStage;
uint16_t programStagesNumber;
bool started;

unsigned long endMillis;
unsigned long programStartMillis;
float totalMinutes;
int16_t programsNumber;
int16_t program;
float globalDefaultDelay = 3;
float programDefaultDelay = globalDefaultDelay;
bool buzz = false;

namespace sd {
  File file;
 
  void seek(uint32_t newPos) {
    if(!file.seek(newPos)) {
      file.seek(0);
      file.seek(newPos);
    }
  }
  
  void seekPrev() {
    seek(file.position() - 1);
  }

  char findNextProgram() {
    bool lineStart = file.position() == 0;
    while(file.available()) {
      char ch = file.read();
      //WRITE(ch);
      switch(ch) {
        case '\r':
        case '\n':
          lineStart = true;
          break;
        default:
          if(lineStart) {
            if(isDigit(ch) || ch == ':') {
              //PRINTLN("findNextProgram returned");
              return ch;
            }
          }
          lineStart = false;
      }
    }
    return '\0';
  }
  
  int16_t getProgramsNumber() {
    int16_t num = 0;
    char c;
    while(c = findNextProgram()) num++;
    WRITE("Programs number ");
    PRINT(num, DEC);
    PRINTLN("");
    return num;
  }
  
  float readFrequency() {
    char buff[50];
    int i=0;
    while(file.available()) {
      char ch = file.read();
      if(isDigit(ch) || ch == '.') {
        buff[i++]=ch;
      } else {
        seekPrev();
        break;
      }
    }
    buff[i]='\0';
    return atof(buff);
  }
  
  void skipSpaces(){
    while(file.available()) {
      char ch = file.read();
      if(ch != ' ' && ch != '\t') {
        seekPrev();
        return;
      }
    }
  }
  
  float readDelay() {
    if(file.available()) {
      char ch = file.read();
      if(ch == ' ' || ch == '\t') {
        skipSpaces();
        return programDefaultDelay;
      } else if (ch == '\r' || ch == '\n') {
        seekPrev();
        return programDefaultDelay;
      } else if (ch == ':') {
        char buff[50];
        int i=0;
        while(file.available()) {
          char ch = file.read();
          if(isDigit(ch) || ch == '.') {
            buff[i++]=ch;
          } else {
            seekPrev();
            break;
          }
        }
        buff[i]='\0';
        skipSpaces();
        return atof(buff);
      }
    }
    return programDefaultDelay;
  }

  float readDefaultDelay() {
    if(file.available() && file.peek() == ':') {
        char buff[50];
        int i=0;
        file.read();
        while(file.available()) {
          char ch = file.read();
          if(isDigit(ch) || ch == '.') {
            buff[i++]=ch;
          } else {
            break;
          }
        }
        buff[i]='\0';
        skipSpaces();
        return atof(buff);
    }
    return globalDefaultDelay;
  }
  
  void seekProgram(int16_t program) {
    seek(0);
    for(int i=0; i<=program; i++) {
      findNextProgram();
    }
    seekPrev();
  }
  
  void fillProgramInfo(uint16_t& stages, float& minutes) {
    stages = 0;
    minutes = 0;
    programDefaultDelay = readDefaultDelay();
    
    uint32_t progStartPos = file.position();
    do{
      char ch = file.peek();
      if(ch == '\r' || ch == '\n') {
        break;
      }
      float f = readFrequency();
      stages++;
      float d = readDelay();
      minutes += d;
      
      PRINT(stages, DEC);
      WRITE(") ");
      PRINT(f, DEC);
      WRITE(":");
      PRINT(d, DEC);
      PRINTLN();
    } while(file.available());
    PRINTLN();
    seek(progStartPos);
  }
}  

void setStarted(bool st) {
  started = st;
  endMillis = 0;
#if(!DEBUG)
  digitalWrite(pinProgStarted, st?HIGH:LOW);
#endif
}

//Запись в AD9833
void WriteAD9833(uint16_t Data){
  SPI.beginTransaction(SPISettings(SPI_CLOCK_DIV2, MSBFIRST, SPI_MODE2));
  digitalWrite(genChipSelect, LOW);
  delayMicroseconds(1);
  SPI.transfer16(Data);
  digitalWrite(genChipSelect, HIGH);
  SPI.endTransaction();
}

void printError(const char* err) {
  lcd.setCursor(0, 0);
  lcd.print(err);
  lcd.setCursor(0, 1);
  lcd.print("                 ");
}

void resetScreen() {
  lcd.setCursor(0, 0);
  lcd.print("                 ");
  lcd.setCursor(0, 1);
  lcd.print("                 ");
  lcd.setCursor(0, 0);
}

void paint(bool inputMode = false) {
  resetScreen();
  if(inputMode) {
    lcd.setCursor(0, 1);
    lcd.print("* Start  # Reset");
    lcd.setCursor(0, 0);
    lcd.print("Input prog: ");
    lcd.cursor();
  } else {
    lcd.print("Prog ");
    lcd.noCursor();
  }
  if(program >= 0) lcd.print(program + 1, DEC);
  if(started) {  
    lcd.setCursor(9, 0);
    lcd.print("Step ");
    lcd.print(programStage + 1, DEC);
    lcd.setCursor(0, 1);
    //lcd.print("F");
    lcd.print(programFrequency, 2);
    lcd.print("Hz ");
    //lcd.setCursor(12, 1);
    //lcd.print("T");
    lcd.print(programDelay, 1);
    lcd.print("m");
  }
}

void paintScreen1() {
  resetScreen();
  lcd.print("Step ");
  lcd.print(programStage + 1, DEC);
  lcd.print("/");
  lcd.print(programStagesNumber);
  lcd.setCursor(0, 1);
  lcd.print("Time ");
  lcd.print(float(millis() - programStartMillis) / 60000, 1);
  lcd.print("m/");
  lcd.print(totalMinutes, 1);
  lcd.print("m");
}

void repaint() {
  static uint32_t lastRepaintTime = millis();
  static bool lastScreen = false;
  if(millis() - lastRepaintTime > 3000) {
    if(lastScreen) {
      paint();
    } else {
      paintScreen1();
    }
    lastScreen = !lastScreen;
    lastRepaintTime = millis();
  }
}

// Установить частоту
void setFrequency(float val) {
  WRITE("Set frequency: ");
  PRINT(val, DEC);
  PRINTLN();
  programFrequency = val;
  
  WriteAD9833(bCntrl_reg | bReset | bB28);
  if(val > 0) {
    unsigned long FreqData = round(val * 10.73741824/* + 0.5*/);
    WriteAD9833(FreqData & 0x3FFF | bFreq_reg0);
    WriteAD9833((FreqData >> 14) | bFreq_reg0);
    WriteAD9833(bCntrl_reg | bB28); // Снимаем Reset
  }
}

// Установить длительность
void setDelay(float seconds) {
  programDelay = seconds;
  endMillis = millis() + seconds * 1000L * 60;
}

void startProgram() {
#if(!DEBUG)
  pinMode(pinProgStarted, OUTPUT);
  pinMode(pinBuzz, OUTPUT);
#endif
  programStagesNumber = 0;
  totalMinutes = 0;
  programStartMillis = millis();
  sd::seekProgram(program);
  sd::fillProgramInfo(programStagesNumber, totalMinutes);
  setFrequency(sd::readFrequency());
  setDelay(sd::readDelay());

  programStage = 0;
  stopBuzz();
}

void stopProgram() {
  endMillis = 0;
  program = -1;
  setStarted(false);
  setFrequency(0);
  paint(true);
  buzz = true;
}

void nextFrequency() {
  programStage++;
  if(programStage == programStagesNumber) {
    endMillis = 0;
    stopProgram();
  } else {
    setFrequency(sd::readFrequency());
    setDelay(sd::readDelay());
  }
}

void inputProgram() {
  static int16_t num1=0;
  char button = NO_KEY;
  // Здесь мы читаем клавиатуру и составляем число.
  // Цикл выполняется, пока мы не нажмем кнопку '*', и цикл прервется,
  // или, если будет нажата кнопка '#', всё начнется с начала. 
      
  button = keypad.getKey(); // Чтение кнопки
  switch(button) {
    case NO_KEY:
      return;
      //continue;
    case '#': // Если пользователь хочет сбросить набор первого числа
      num1 = 0;
      break;      
    case '*': // Если пользователь завершил ввод цифр
      break;
    default:
      num1 = num1*10 + (button -'0'); 
      break;      
  }
  WRITE("Pressed ");PRINTLN(button);
  if (num1 > programsNumber)
  {
    num1 = 0;
  }
  program = num1 - 1;
  paint(true);
  stopBuzz();
  if(button == '*' && program >= 0) {
    setStarted(true);
    num1 = 0;
  }
}

void updateBuzz() {
#if !DEBUG
  if(buzz) {
    static uint32_t lastUpdateTime = millis();
    if(millis() - lastUpdateTime > 500) {
      digitalWrite(pinBuzz, !digitalRead(pinBuzz));
      lastUpdateTime = millis();
    }
  }
#endif
}

void stopBuzz() {
#if !DEBUG
  buzz = false;
  digitalWrite(pinBuzz, LOW);
#endif
}

bool error = false;

void setup() {
  SERIAL_BEGIN(9600);
  // Инициализация синтезатора частоты
  SPI.begin();
  // Инициализация индикатора
  // устанавливаем размер (количество столбцов и строк) экрана
  lcd.begin(16, 2);
  printError("   Loading...    ");
  if (!SD.begin(sdChipSelect)) {
    printError(" SD Card failed  ");
    error = true;
    return;
  }
  sd::file = SD.open("/raif.cfg", FILE_READ );
  if (!sd::file) {
    printError("File read failed ");
    error = true;
    return;
  }
  programsNumber = sd::getProgramsNumber();
  if (programsNumber == 0) {
    printError("Read config error");
    error = true;
    return;
  }
  stopProgram();
}

void loop() {
  if(error) return;
  if(started) {
    if(endMillis == 0) {
      startProgram();
    } else if (millis() >= endMillis) {
      nextFrequency();
    }
    repaint();
    if(keypad.getKey() == '#') {
      stopProgram();
      stopBuzz();
    }
  } else {
    updateBuzz();
    inputProgram();
  }
}
