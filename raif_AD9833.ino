unsigned int data[] = {
27, 
2720, 2489, 2170, 2000, 1865, 1800, 1600, 1550, 880, 832, 802, 787, 776, 727, 660, 465, 450, 444, 440, 428, 380, 250, 146, 125, 95, 72, 20, 1.2
5,    5,    5,    5,    5,    5,    5,    5,    5,   5,   5,   5,   5,   5,   5,   5,   5,   5,   5,   5,   5,   5,   5,   5,   5,  5,  5,  5    
};

//https://tsibrov.blogspot.com/2018/06/ad9833.html
#include <SPI.h>
// подключаем библиотеку LiquidCrystalRus
#include <LiquidCrystalRus.h>
// библиотека клавиатуры
#include <Keypad.h>


// инициализируем объект-экран, передаём использованные 
// для подключения контакты на Arduino в порядке:
// RS, E, DB4, DB5, DB6, DB7
LiquidCrystalRus lcd(6, 7, 5, 4, 3, 2);

const byte ROWS = 4; 
const byte COLS = 3; 
char keys[ROWS][COLS] = {
  {'1','2','3'},
  {'4','5','6'},
  {'7','8','9'},
  {'*','0','#'}
};
byte rowPins[ROWS] = {14, 15, 16, 17}; 
byte colPins[COLS] = {18, 8, 9}; 
 
Keypad keypad = Keypad( makeKeymap(keys), rowPins, colPins, ROWS, COLS );
int state = 0;
unsigned long startMillis = 0;

void WriteAD9833(uint16_t Data){
  SPI.beginTransaction(SPISettings(SPI_CLOCK_DIV2, MSBFIRST, SPI_MODE2));
  digitalWrite(SS, LOW);
  delayMicroseconds(1);
  SPI.transfer16(Data);
  digitalWrite(SS, HIGH);
  SPI.endTransaction();
}

// обработать события:
void keypadEvent(KeypadEvent key){
  switch (keypad.getState()){
    case PRESSED:
    break;
    case RELEASED:
      switch (key){
        case '*': 
          digitalWrite(ledPin,!digitalRead(ledPin));
          blink = false;
        break;
      }
    break;
    case HOLD:
    break;
  }
}

void setup() {
  // Инициализация синтезатора частоты
  SPI.begin();
  WriteAD9833(0x2100); //0010 0001 0000 0000 - Reset + DB28
  WriteAD9833(0x50C7); //0101 0000 1100 0111 - Freq0 LSB (4295)
  WriteAD9833(0x4000); //0100 0000 0000 0000 - Freq0 MSB (0)
  WriteAD9833(0xC000); //1100 0000 0000 0000 - Phase0 (0)
  WriteAD9833(0x2000); //0010 0000 0000 0000 - Exit Reset

  // Инициализация индикатора
  // устанавливаем размер (количество столбцов и строк) экрана
  lcd.begin(16, 2);
  //инициализация клавиатуры
  keypad.addEventListener(keypadEvent); // добавить слушателя события для данной клавиатуры
}

void loop() {
  WriteAD9833(0x2000); //0010 0000 0000 0000 - Синусоидальный сигнал
  delay(5000);
  WriteAD9833(0x2002); //0010 0000 0000 0010 - MODE=1 - Треугольный
  delay(5000);
  WriteAD9833(0x2020); //0010 0000 0010 0000 - OPBITEN=1 - Прямоугольный (MSB/2)
  delay(5000);
  WriteAD9833(0x2028); //0010 0000 0010 1000 - OPBITEN=1, DIV2=1 - Прямоугольный (MSB)
  delay(5000);
}
