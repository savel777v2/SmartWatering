// Требуется подключение следующий библиотек
// - DHT Sensor Library: https://github.com/adafruit/DHT-sensor-library
// - Adafruit Unified Sensor Lib: https://github.com/adafruit/Adafruit_Sensor

#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>

// ОТЛАДКА
#define DEBUGGING true // для включения отладки
#define MINUTE_DURATION 60000 // длительность минуты, в рабочем режиме используем 60000 (минута), для отладка "ускоряем" до 100 (мс)

// Датчик влажности почвы инверсный: больше влажность - меньше значение.
#define SENSOR_PIN A0 // аналоговый вход датчика влажности
#define MIN 545 // Определяем минимальное показание датчика (в воздухе),
#define MAX 192 // определяем максимальное показание датчика (в воде),

// Датчик влажности воздуха + температуры DHT22
#define DHT22_PIN 2 // цифровой вход датчика влажности
DHT_Unified Dht(DHT22_PIN, DHT22);

// аналоговые выходы управления насосами, например для полива подаем ненулевой вольтаж
#define MOTOR_PIN_1 A1 // насос 1 капельного орошения
#define MOTOR_PIN_2 A2 // насос 2 спринклерного орошения
// Текущий режим орошения: индекс соответвует номеру насоса
//  первое значение - режим орошения (0 - отключен, 1 - включен)
//  второе значение - текущая продолжительность в минутах режима,
//  третье значение - нужная продолжительность в минутах режима
int modeWatering[2][3];
// Счетчик минут в целях срабатывания режимов
int counterMunuteMode[3];
// настройка констант времени алгоритмов
#define LOOP_MINUTE_FOR_0 1440 // 24 * 60 24 часа
#define DURATION_MINUTE_FOR_1 60 // 1 час
#define DURATION_MINUTE_FOR_2 40 // 40 минут

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
  pinMode(MOTOR_PIN_1, OUTPUT);
  analogWrite(MOTOR_PIN_1, 0);
  pinMode(MOTOR_PIN_2, OUTPUT);
  analogWrite(MOTOR_PIN_2, 0);
}

void loop() {
  loopTime();
  loopWatering();
}


void loopWatering() {
  static unsigned long _lastLoopTime[2] = {0, 0}; // последнее время обработки каждого насоса

  for (int i = 0; i <= 1; i++) {

    if (modeWatering[i][0] == 0 && _lastLoopTime[i] > 0) {
      // обработка отключения режима например убираем сигнал с ножки
      // отработает единожды после отключения полива
      if (DEBUGGING) {
        Serial.print("Watering ");
        Serial.print(i);
        Serial.println(" OFF");
      }
      if (i == 0) analogWrite(MOTOR_PIN_1, 0);
      else analogWrite(MOTOR_PIN_2, 0);      
      _lastLoopTime[i] = 0;
      modeWatering[i][1] = 0;
      modeWatering[i][2] = 0;
    }

    if (modeWatering[i][0] != 0) {
      if (_lastLoopTime[i] == 0) {
        // обработка включения режима например ставим сигнал на ножку
        // отработает единожды при включении полива
        if (DEBUGGING) {
          Serial.print("Watering ");
          Serial.print(i);
          Serial.println(" ON");
        }
        if (i == 0) analogWrite(MOTOR_PIN_1, 400); // уровень сигнала для насоса 1
        else analogWrite(MOTOR_PIN_2, 400); // уровень сигнала для насоса 1
      }
      // По умолчанию цикл - минута
      if ((millis() - _lastLoopTime[i]) > MINUTE_DURATION) {
        _lastLoopTime[i] = millis();
        modeWatering[i][1]++;
        // условие завершения полива по времени
        if (modeWatering[i][1] >= modeWatering[i][2]) modeWatering[i][0] = 0;
      }
    }
  }
}

// прцоедура каждую минуту считывает показания сенсоров и анализирует
// алгоритмы включения полива. При необходимости включает режим полива
// в соответсвии с алгоритмом modeWatering = индекс алгоритма (с нуля) + 1
void loopTime() {
  static unsigned long _lastLoopTime = 0; // последнее время обработки этого алгоритма
  sensors_event_t _eventDht;

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
    if (counterMunuteMode[0] > LOOP_MINUTE_FOR_0 && modeWatering[0][0] == 0) {
      // если по факту уже поливается, то он обождет окончание полива и включит свою программу.
      counterMunuteMode[0] = 0;
      // включим первый насос на 3 часа
      modeWatering[0][0] = 1;
      modeWatering[0][1] = 0;
      modeWatering[0][2] = 180;
    }

    // анализ алгоритма 1 - сигнал от датчика влажности почвы меньше 70% в течении DURATION_MINUTE_FOR_1 минут
    if (sensorsData.soilMoisture <= 70) counterMunuteMode[1]++;
    else  counterMunuteMode[1] = 0;
    if (counterMunuteMode[1] > DURATION_MINUTE_FOR_1 && modeWatering[0][0] == 0) {
      // если по факту уже поливается, то он обождет окончание полива и включит свою программу если условия за время полива не изменились
      counterMunuteMode[1] = 0;
      // включим первый насос на 3 часа
      modeWatering[0][0] = 1;
      modeWatering[0][1] = 0;
      modeWatering[0][2] = 180;
    }

    // анализ алгоритма 2 - низкая влажность воздуха и превышение температуры воздуха в течении DURATION_MINUTE_FOR_2 минут
    if (sensorsData.relative_humidity <= 40 && sensorsData.temperature >= 30) counterMunuteMode[2]++;
    else counterMunuteMode[2] = 0;
    if (counterMunuteMode[2] > DURATION_MINUTE_FOR_2 && modeWatering[1][0] == 0) {
      // если по факту уже поливается, то он обождет окончание полива и включит свою программу если условия за время полива не изменились
      counterMunuteMode[2] = 0;
      // включим второй насос на 2 минуты
      modeWatering[1][0] = 1;
      modeWatering[1][1] = 0;
      modeWatering[1][2] = 2;
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
  Serial.print(" modeWatering[0] вкл: ");
  Serial.print(modeWatering[0][0]);
  Serial.print(" мин: ");
  Serial.print(modeWatering[0][1]);
  Serial.print(" макс: ");
  Serial.print(modeWatering[0][2]);
  Serial.print(" modeWatering[1] вкл: ");
  Serial.print(modeWatering[1][0]);
  Serial.print(" мин: ");
  Serial.print(modeWatering[1][1]);
  Serial.print(" макс: ");
  Serial.print(modeWatering[1][2]);
  Serial.println(" ");
}
