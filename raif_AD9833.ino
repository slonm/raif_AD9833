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
const byte rowPins[ROWS] = {14, 15, 16, 17}; 
const byte colPins[COLS] = {18, 19, 8}; 
//SPI SD card
const byte sdChipSelect = 9;
//SPI AD9833
const byte genChipSelect = SS;

//https://tsibrov.blogspot.com/2018/06/ad9833.html
#include <SPI.h>
// подключаем библиотеку LiquidCrystal
#include <LiquidCrystal.h>
// библиотека клавиатуры
#include <Keypad.h>
// библиотека поддержки SD карты
#include <SD.h>

#if 0
  #define SERIAL_BEGIN(a) Serial.begin(a);  while (!Serial);
  #define WRITE(a) Serial.write(a)
  #define PRINTLN() Serial.println()
  #define PRINTLN(a) Serial.println(a)
  #define PRINT(a, b) Serial.print(a,b)
#else
  #define SERIAL_BEGIN(a)
  #define WRITE(a)
  #define PRINTLN()
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

float programFrequency[50];
const float* pProgramFrequency;
byte programDelay[50];
const byte* pProgramDelay;
uint16_t programStage;
uint16_t programStagesNumber;
bool started;

unsigned long endMillis;
unsigned long programStartMillis;
unsigned long totalMinutes;
int16_t programsNumber;
int16_t program;

//Обход всех программ / поиск программы по номеру
int16_t iterate_all(bool(*Cb)(int16_t, char*)) {
  File file = SD.open("/raif.cfg", FILE_READ);
  if (!file) {
    return -1;
  }
  bool lineStart = true;
  bool isDataLine = false;
  int linePos = 0;
  char programLine[256];
  int16_t currentProgram = -1;
  while(file.position() < file.size()) {
    char ch = file.read();
    switch(ch) {
      case '\r':
      case '\n':
        lineStart = true;
        linePos = 0;
        if(isDataLine) {
          if(Cb(currentProgram, programLine)) {
            return currentProgram;
          }
        }
        isDataLine = false;
        break;
      default:
        if(lineStart && isDigit(ch)) {
           isDataLine = true;
           currentProgram++;
        }
        lineStart = false;
        if(isDataLine) {
           programLine[linePos++] = ch;
           programLine[linePos] = '\0';
        }
    }
  }
  file.close();
  return currentProgram;
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
    lcd.print("F");
    lcd.print(*pProgramFrequency);
    lcd.print("Hz");
    lcd.setCursor(12, 1);
    lcd.print("T");
    lcd.print(*pProgramDelay, DEC);
    lcd.print("m");
  }
}

void paintScreen1() {
  resetScreen();
  lcd.print("Step ");
  lcd.print(programStage + 1, DEC);
  lcd.print(" of ");
  lcd.print(programStagesNumber);
  lcd.setCursor(0, 1);
  lcd.print("Time ");
  lcd.print((millis() - programStartMillis) / 60000, DEC);
  lcd.print("m of ");
  lcd.print(totalMinutes);
  lcd.print("m");
}

// Установить частоту
void setFrequency(float val) {
  WriteAD9833(bCntrl_reg | bReset | bB28);
  unsigned long FreqData = round(val * 10.73741824/* + 0.5*/);
  WriteAD9833(FreqData & 0x3FFF | bFreq_reg0);
  WriteAD9833((FreqData >> 14) | bFreq_reg0);
  WriteAD9833(bCntrl_reg | bB28); // Снимаем Reset
}

// Установить длительность
void setDelay(byte seconds) {
  endMillis = millis() + seconds * 1000L * 60;
}

void startProgram() {
  programStagesNumber = 0;
  totalMinutes = 0;
  programStartMillis = millis();
  iterate_all([](int16_t currentProgram, char* programLine){
    if(currentProgram == program) {
      char * pch = strtok(programLine," \t");
      while (pch != NULL) {
        programFrequency[programStagesNumber] = atof(pch);
        char * pch2 = pch;
        while(*pch2 && *pch2 != ':')pch2++;
        if(*pch2) {
          programDelay[programStagesNumber] = atoi(++pch2);
        } else {
          programDelay[programStagesNumber] = 5;
        }
        totalMinutes += programDelay[programStagesNumber];
        PRINT(programFrequency[programStagesNumber], DEC);
        WRITE(" ");
        PRINTLN(programDelay[programStagesNumber]);
        pch = strtok(NULL, " \t");
        programStagesNumber++;
      }
      return true;
    }
    return false;
  });
  
  programStage = 0;
  pProgramFrequency = programFrequency;
  pProgramDelay = programDelay;
  setDelay(*pProgramDelay);
  setFrequency(*pProgramFrequency);
  paint();
}

void stopProgram() {
  endMillis = 0;
  program = -1;
  started = false;
  setFrequency(0);
  paint(true);
}

void nextFrequency() {
  programStage++;
  if(programStage == programStagesNumber) {
    endMillis = 0;
    stopProgram();
  } else {
    pProgramFrequency++;
    pProgramDelay++;
    setFrequency(*pProgramFrequency);
    setDelay(*pProgramDelay);
    paint();
  }
}

void inputProgram() {
  int16_t num1=0;
  char button = NO_KEY;
  while(button != '*' || program < 0) {
    // Здесь мы читаем клавиатуру и составляем число.
    // Он выполняется, пока мы не нажмем кнопку '*', и цикл прервется,
    // или, если будет нажата кнопка '#', всё начнется с начала. 
        
    button = keypad.getKey(); // Чтение кнопки
    switch(button) {
      case NO_KEY:
        continue;
      case '#': // Если пользователь хочет сбросить набор первого числа
        num1 = 0;
        break;      
      case '*': // Если пользователь завершил ввод цифр
        break;
      default:
        num1 = num1*10 + (button -'0'); 
        break;      
    }
    PRINTLN(button);
    if (num1 > programsNumber)
    {
      num1 = 0;
    }
    program = num1 - 1;
    paint(true);
  }
  started = true;
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
  programsNumber = iterate_all([](int16_t currectProgram, char* programLine){
    PRINT(currectProgram, DEC);
    WRITE(": ");
    PRINTLN(programLine);
    return false;
  });
  if (programsNumber < 0) {
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
    } else {
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
    if(keypad.getKey() == '#') {
      stopProgram();
    }
  } else {
    inputProgram();
    endMillis = 0;
  }
}
