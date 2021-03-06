Устройство читает список программ из конфигурационного файла raif.cfg, который необходимо поместить в корень файловой системы SD карты.

## Формат конфигурационного файла

Каждая строка файла, которая начинается с числового символа или двоеточия, считается описанием лечебной программы.
Все остальные строки игнорируются при чтении.
Программы нумеруются подряд по мере обнаружения их в конфигурационном файле с единицы.

Строка лечебной программы состоит из пар <частота[:длительность]>, разделенных пробелами.

Частота записывается в герцах а формате с плавающей точкой.
Опциональная длительность записывается в целых минутах в пределах 1-255 мин. Длительность по умолчанию 5 мин.
В самом начале строки может быть указана длительность по умолчанию для всех частот программы.

### Пример

```
#1 program

#Musical notes
261.63:1 293.66:2 329.63:3 349.23:4 392.00:5 440.00:6 493.88:7
:1.5 2720 2489 2170 2000 1865 1800 1600 1550 880 832 802 787 776 727 660 465 450 444 440 428 380 250 146 125 95 72 20 1.2
10000 3000 95 3
2720 2170 1865 1550 880 802 787 727 500 444 190
#5 program
2720 2170 1865 1550 880 802 787 760 727 690 660 500 465 450 444 428 190
```
строки 1, 2, 3, 8 игнорируются

строки 4, 5, 6, 7, 9 программные строки

строка 4 состоит из 7ми частот. Для каждой частоты указана своя длительность после знака :

строка 5 состоит из 28 частот. Все частоты имеют длительность 1,5 минуты

строка 6 состоит из 4х частот. Все частоты имеют длительность 5 минут

## Описание файлов
*raif_AD9833.ino* скетч Arduino

*raif_AD9833_bb.png* схема монтажная

*raif.cfg* конфигурационный файл для записи на SD карту

*sd-pin-description-2w.png* распиновка SD карты

*sd-pin-description-4w.png* распиновка mini SD карты


## Ссылки

- [Таблица частот Райфа](http://khoroshih.com/?p=8162)

- [Генератор сигналов на AD9833](https://tsibrov.blogspot.com/2018/06/ad9833.html)

- [Работа с SD картой. Подключение к микроконтроллеру](http://chipenable.ru/index.php/programming-avr/209-rabota-s-sd-kartoy-podklyuchenie-k-mikrokontrolleru-ch1.html)
