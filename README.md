## NodeMCU azimuth control

Контроллер поворота антенны на модуле NodeMCU.
>Это мой первый опыт в разработке веб интерфейсов, поэтому что-то может выглядеть немного криво или из-за разных разрешений экрана или  какие-то блоки на страницах могут стоять "не на своих местах" и т.п. С этми буду по возможности разбираться в процессе доработок.

#### Что умеет?
- Управлять приводом поворотного устройства
- Расчитывать текущий азимут антенны по напряжению с датчика угла поворота
- Расчитывать азимут по QTH локаторам
- Поворачивать антенну до заданного азимута
- Учитывать иннерционность привода
- Выключать привод по таймауту если заданный азимут не достигнут по какой-либо причине
- Выключать привод если азимут больше 365 или меньше -5 градусов
- Работает как точка доступа или клиент WiFi
- Настройка клиента WiFi через web интерфейс
- Страница настройки и калибровки
- Сохранять настройки

#### Что необходимо?
- Плата NodeMCU на базе ESP8266
- Пара транзисторов для управления реле включения привода
- Источник питания для NodeMCU
- Среда программирования Arduino IDE с установленными библиотеками:
  - ESP8266FtpServer.h - есть в репозитории
  - ESP8266WiFi.h
  - ESP8266WebServer.h
  - FS.h
  - ArduinoJson.h - версии 5.13.5 (есть в менеджере библиотек)
- Программа WinSCP для загрузки файлов вебинтерфейса
  
#### Схема устройства
Думаю что схему рисовать пока нет смысла, потому что из "обвязки" у модуля NodeMCU только транзисторы для управления реле исполнительного механизма. Описание выводов представлено ниже:
* D0 (GPIO16) - поворот привода в лево
* D1 (GPIO5) - поворот привода в право
* D2 (GPIO4) - сигнал - подключение по WIFI установлено, можно подключить светодиод через резистор
* D3 (GPIO0) - сигнал - низкий уровень RSSI (для режима клиента), можно подключить светодиод через резистор
* GND
* Vin - питание модуля
* A0 - вход напряжения с датчика угла поворота

#### Принцип работы
Устройство измеряет напряжение пропорциональное углу поворота антенны, пересчитывает его в азимут, и отображает на веб странице. В качестве датчика угла поворота может быть использовано любое устройство выходное напряжение которого пропорционально углу поворота. Максимальное напряжение подаваемое на модуль зависит от делителя напряжения установленного перед модулем ESP8266. Непосредственно на модуль ESP можно поадвать напряжение не выше 1 вольта, поэтому диапазон изменения напряжения, подаваемого на модуль ESP, с учётом коэффициента деления должен лежать в пределах 0..1 В. Перед использованием устройство нужно откалибровать. Запоминаются максимальное и минимальное значения с датчика угла поворота, соответствующие либо 0 градусов либо 360. По полученному значению расчитывается значение изменения напряжения на 1 градус, и далее по абсолютному изменению напряжения (относительно минимального значения с датчика) расчитывается текущий азимут.

#### Настройка и начало работы
После компиляции и загрузки скетча модуль по умолчанию будет работать в режиме точки доступа. SSID - "ESPAP" и паролем - 87654321 (пароль для точки доступа пока можно поменять только в скетче). Соединяемся с точкой доступа, в свойствах беспроводного соединения нужно задать IP адреc ПК с которого происходит соединение - 192.168.4.2 маска 255.255.255.0. Далее необходимо с помощью программы WinSCP через встроенный ftp сервер записать файлы веб интерфейса из папки "web files". Логин и пароль для подключения к ftp - "ham", "ham". Далее в браузере переходим по адресу 192.168.4.1. Должны увидель основную страницу интерфейса. В левом нижнем углу выводится строка состояния, и там же есть ссылка на страницу установок  - "Установки". На этой странице можно настроить устройство как WiFi клиент для доступа к нему из домашней сети. Изменения вступают в силу после перезагрузки устройства, на этой же странице есть кнопка перезагрузки устройства. Или можно воспользоваться кнопкой ресет на плате NodeMCU. IP адрес выданый устройству в домашней сети можно узнать либо при страте в "Мониторе порта" в Arduino IDE. Либо подключившись к точке доступа ESPAP на странице настроек.

#### Калибровка
На странице установок есть блок для калибровки устройства. Для калибровки сначала необходимо установить антенну в положение с минимальным выходным напряжением с датчика и нажать кнопку Установить напротив строки "Минимальное напряжение датчика, В". Далее повернуть антенну в положение с максимальным выходным напряжением с датчика и нажать кнопку Установить напротив строки "Максимальное напряжение датчика, В". Сохранённые значения должны отобразиться напртив соответствующих строк таблицы. А так же должно быть расчитано значение Вольт на градус. В том случае если напряжение с датчика уменьшается при увеличении значения азимута, то необходимо установить значение "Инверсия" в состояние True.
