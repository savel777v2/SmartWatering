// Требуется подключение следующий библиотек
// - DHT Sensor Library: https://github.com/adafruit/DHT-sensor-library
// - Adafruit Unified Sensor Lib: https://github.com/adafruit/Adafruit_Sensor

#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>

// ОТЛАДКА
#define DEBUGGING true // для включения отладки
#define MINUTE_DURATION 1000 // длительность минуты, в рабочем режиме используем 60000 (минута), для отладка "ускоряем" до 1000 (секунда)

// Датчик влажности почвы инверсный: больше влажность - меньше значение.
#define SENSOR_PIN A0 // аналоговый вход датчика влажности
#define MIN 545 // Определяем минимальное показание датчика (в воздухе),
#define MAX 192 // определяем максимальное показание датчика (в воде),

// Датчик влажности воздуха + температуры DHT22
#define DHT22_PIN 2 // цифровой вход датчика влажности
DHT_Unified Dht(DHT22_PIN, DHT22);

// аналоговый выход управления мотором, например для полива подаем ненулевой вольтаж
#define MOTOR_PIN A1

int modeWatering; // режим орошения 0 - отключен
// Настройка режима орошения индекс соответвует modeWatering-1, первое значение - выбор уровня сигнала, второе значение - продолжительность в минутах
int settModeWatering[3][2] = {{2, 5}, {2, 10}, {4, 60}};
int counterMunuteMode[3];
// настройка констант времени алгоритмов
#define LOOP_MINUTE_FOR_0 48 * 60
#define DURATION_MINUTE_FOR_1 24 * 60
#define DURATION_MINUTE_FOR_2 3 * 60

struct {
  int sensorLevel = 0;
  int soilMoisture = 0;
  float temperature = 0;
  float relative_humidity = 0;
} sensorsData;

void setup() {

  if (DEBUGGING) Serial.begin(9600);
  Dht.begin();
  sensor_t sensor;
  Dht.temperature().getSensor(&sensor);
  Dht.humidity().getSensor(&sensor);
  pinMode(MOTOR_PIN, OUTPUT);
  analogWrite(MOTOR_PIN, 0);
}

void loop() {
  loopTime();
  loopWatering();
}


void loopWatering() {
  static unsigned long _lastLoopTime = 0; // последнее время обработки этого алгоритма
  static int _counterMunute = 0; // счетчик минут продожтельности полива

  if (modeWatering == 0 && _lastLoopTime > 0) {
    // обработка отключения режима например убираем сигнал с ножки
    // отработает единожды после отключения полива
    if (DEBUGGING) Serial.println("Watering OFF");
    analogWrite(MOTOR_PIN, 0);    
    _lastLoopTime = 0;
    _counterMunute = 0;
  }

  if (modeWatering != 0) {
    if (_lastLoopTime == 0) {
      // обработка включения режима например ставим сигнал на ножку
      // отработает единожды при включении полива
      if (DEBUGGING) Serial.println("Watering ON");
      switch (settModeWatering[modeWatering - 1][0]) {
        case 2:
          // уровень сигнала для 0,2 МПа
          analogWrite(MOTOR_PIN, 200);
          break;
        case 4:
          // уровень сигнала для 0,4 МПа
          analogWrite(MOTOR_PIN, 400);
          break;
      }
    }
    // По умолчанию цикл - минута
    if ((millis() - _lastLoopTime) > MINUTE_DURATION) {
      _lastLoopTime = millis();
      _counterMunute++;
      // условие завершения полива по времени
      if (_counterMunute >= settModeWatering[modeWatering - 1][1]) modeWatering = 0;
    }
  }
}

// прцоедура каждую минуту считывает показания сенсоров и анализирует
// алгоритмы включения полива. При необходимости включает режим полива
// в соответсвии с алгоритмом modeWatering = индекс алгоритма (с нуля) + 1
void loopTime() {
  static unsigned long _lastLoopTime = 0; // последнее время обработки этого алгоритма
  sensors_event_t _eventDht;
  static int _counterMunute0 = 0; // счетчик минут для алгоритма 0
  static int _counterMunute1 = 0; // счетчик минут для алгоритма 1
  static int _counterMunute2 = 0; // счетчик минут для алгоритма 2

  // По умолчанию цикл - минута
  if ((millis() - _lastLoopTime) > MINUTE_DURATION) {

    _lastLoopTime  = millis();

    // минута оттикала - считаем датчики
    sensorsData.sensorLevel = analogRead(SENSOR_PIN);             // Читаем сырые данные с датчика влажности почвы,
    sensorsData.soilMoisture = map(sensorsData.sensorLevel, MIN, MAX, 0, 100);  // адаптируем значения от 0 до 100,
    Dht.temperature().getEvent(&_eventDht); // считаем температуру
    if (!isnan(_eventDht.temperature)) sensorsData.temperature = _eventDht.temperature;
    Dht.humidity().getEvent(&_eventDht); // считаем влажность, так то без надобности но вдруг понадобиться
    if (!isnan(_eventDht.relative_humidity)) sensorsData.relative_humidity = _eventDht.relative_humidity;

    // анализ алгоритма 0 - простой полив в цикле каждые LOOP_MINUTE_FOR_0 минут
    counterMunuteMode[0]++;
    if (counterMunuteMode[0] > LOOP_MINUTE_FOR_0 && modeWatering == 0) {
      // если по факту уже поливается, то он обождет окончание полива и включит свою программу.
      counterMunuteMode[0] = 0;
      modeWatering = 1;
    }

    // анализ алгоритма 1 - сигнал от датчика влажности почвы более 100% в течении DURATION_MINUTE_FOR_1 минут
    // СТРАННО >= 100 - это уже болото, заменил на <=10 -сухо, иначе зальем все...
    if (sensorsData.soilMoisture <=10) counterMunuteMode[1]++;
    else  counterMunuteMode[1] = 0;
    if (counterMunuteMode[1] > DURATION_MINUTE_FOR_1 && modeWatering == 0) {
      // если по факту уже поливается, то он обождет окончание полива и включит свою программу если условия за время полива не изменились
      counterMunuteMode[1] = 0;
      modeWatering = 2;
    }

    // анализ алгоритма 2 - низкая влажность воздуха и превышение температуры воздуха в течении DURATION_MINUTE_FOR_2 минут
    if (sensorsData.relative_humidity <= 40 && sensorsData.temperature >= 35) counterMunuteMode[2]++;
    else counterMunuteMode[2] = 0;
    if (counterMunuteMode[2] > DURATION_MINUTE_FOR_2 && modeWatering == 0) {
      // если по факту уже поливается, то он обождет окончание полива и включит свою программу если условия за время полива не изменились
      counterMunuteMode[2] = 0;
      modeWatering = 3;
    }

    // отладка всех переменных
    if (DEBUGGING) printLog();
  }
}

// фунция вывод в порт информации для отладки
void printLog() {
  Serial.print("sensorLevel: ");
  Serial.print(sensorsData.sensorLevel);
  Serial.print(" soilMoisture: ");
  Serial.print(sensorsData.soilMoisture);
  Serial.print("% Temperature: ");
  Serial.print(sensorsData.temperature);
  Serial.print("°C Humidity: ");
  Serial.print(sensorsData.relative_humidity);
  Serial.print("% Munute 0: ");
  Serial.print(counterMunuteMode[0]);
  Serial.print(" Munute 1: ");
  Serial.print(counterMunuteMode[1]);
  Serial.print(" Munute 2: ");
  Serial.print(counterMunuteMode[2]);
  Serial.print(" modeWatering: ");
  Serial.print(modeWatering);
  Serial.println(" ");
}
