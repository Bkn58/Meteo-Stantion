/*
* Метеостанция
* - температура
* - влажность
* - давление
* - часы реального времени
* - датчик загазованности (CO,CO2,alhogol,aceton...)
* - дисплей LCD 16х2 или TFT 128х64
* - карта памяти SD для записи показаний датчиков
*/
#define SERIAL              // включает отладочную печать в Serial
#include "iarduino_DHT.h"   // подключаем библиотеку для работы с датчиком DHT
#include "SFE_BMP180.h"     // датчик давления
#include <Wire.h>

#define LEN_STATUS_STRING 40  // длина буфера статусной строки
#define DHTPIN 7              // пин подключения контакта DATA
#define DHTTYPE DHT11         // DHT 11
iarduino_DHT dht(DHTPIN);     // объявляем  переменную для работы с датчиком DHT, указывая номер цифрового вывода к которому подключён датчик (сейчас 7pin)

#include <LiquidCrystal_I2C.h>
LiquidCrystal_I2C lcd(0x27, 16 , 2 ); //0x27 или 0x3F адрес на шине I2C

const int analogSignal = A1; //подключение аналогового сигнального пина датчика газа MQ135
const int digitalSignal = 6; //подключение цифрового сигнального пина датчика газа MQ135
boolean noGas;               //переменная для хранения значения о присутствии газа
int gasValue = 0;            //переменная для хранения количества газа
boolean heating;             // идет нагрев MQ135


SFE_BMP180 pressure;

// подключение библиотеки для SD
#include <SD.h>
File myFile;

// подключение библиотек для RTC
#include <stdlib.h>
#include <EEPROM.h>
#include <time.h>
#include "Timer_util.h" 

// подключение библиотеки для работы с LOG-файлом
#include "log.h"

//Include the library MQ-135
#include <MQUnifiedsensor.h>

//Definitions
#define placa "Arduino MEGA"
#define Voltage_Resolution 5
#define pin A1 //Analog input 0 of your arduino
#define type "MQ-135" //MQ135
#define ADC_Bit_Resolution 10 // For arduino UNO/MEGA/NANO
#define RatioMQ135CleanAir 3.6//RS / R0 = 3.6 ppm  
//#define calibration_button 13 //Pin to calibrate your sensor

//Declare Sensor
MQUnifiedsensor MQ135(placa, Voltage_Resolution, ADC_Bit_Resolution, pin, type);
boolean calibration;

//include display 128x64
#include <U8g2lib.h>
#include <SPI.h>
U8G2_ST7565_NHD_C12864_F_4W_SW_SPI u8g2(U8G2_R0, /* clock=*/ 13, /* data=*/ 11, /* cs=*/ 10, /* dc=*/ 9, /* reset=*/ 8);
//#include "disp_128x64.h"

/*
 * отобразить дату и время на LCD (I2C)
 */
void showDateTime ()
{
    print2digits(hour, 0, 0);
    lcd.print(":");
    print2digits(minute, 3, 0);
    lcd.print(":");
    print2digits(second, 6, 0);
    print2digits(dayOfMonth, 0, 1);
    lcd.print("/");
    print2digits(month, 3, 1);
    lcd.print("/");
    print2digits(year, 6, 1);
  
}
// процедура вывода на дисплей с добавлением до двух цифр
void print2digits(int number, int col, int str)
{
  lcd.setCursor(col, str);
  if (number >= 0 && number < 10)
  {
    lcd.print ("0");
  }
  lcd.print(number);
}

/*
 * формирует имя файла из текущей даты
 */
char filename[14];
void get_file_name () 
{
  char d_str[3];
  char m_str[3];
  char y_str[3];
  filename[0] = 0;
  //sprintf(filename,"%02d-%02d-%02d.txt",dayOfMonth,month,year);
    strcpy(filename, u8x8_u8toa(dayOfMonth, 2));   // convert day to a string with two digits 
	strcat (filename,"-");
    strcat(filename, u8x8_u8toa(month, 2));		   // convert month to a string with two digits
	strcat (filename,"-");
    strcat(filename, u8x8_u8toa(year, 2));		   // convert year to a string with two digits
	strcat (filename,".txt");
}

double T=0,P=0; // сохраненные значения датчиков температуры и давления

String record; // строка для записи в SD

/*
 * выводит на дисплей показания датчиков, сохраненных в глобальных переменных T,P,dht.hum,gasValue
*/
void showDHT () {
	lcd.clear(); // очистка после отображения времени
	
	lcd.setCursor(0,0);
	lcd.display();
	lcd.print("h:");lcd.print(dht.hum); lcd.print("% ");
	lcd.print("T:"); lcd.print(T);
	
	lcd.setCursor(0,1);
	lcd.print("P:"); lcd.print(P,0);lcd.print(" mmHg"); 
	lcd.print(" ");lcd.print(gasValue);
}
/*
 * Формирует начало строки записи в SD с данных времени
 */
void createRecord ()
{
  record = "";
  if (hour < 10) record += "0";
  record += String(hour) + ":";
  if (minute < 10) record += "0";
  record += String(minute) + ":";
  if (second < 10) record += "0";
  record += String(second);// + ";";
}
boolean isDigital (String& snum) 
{
	for (int i=0;i<snum.length();i++)
	{
		if (!isDigit(snum[i])) return false;
	}
	return true;
}
String inputString = "";		// строка, собираемая из данных, приходящих в последовательный порт
boolean date_timeComplete = false; // флаг комплектности строки
boolean readInProgress = false; // флаг чтения SD по команде из монитора порта "read"
boolean logReadInProgress = false; // флаг чтения log-файла по команде из монитора порта "log"
unsigned long current_pos;      // текущая позиция читаемого байта в файле SD
File root;						// переменная для вывода списка файлов

/*
 *  получение команды по последовательному порту
*/
void serialEvent()
{
#ifdef SERIAL
  String cmd;
  String s_num;
  
  while (Serial.available())
  { // получить очередной байт:
    char inChar = (char)Serial.read();
    // добавить в строку
    if (inChar != '\n') inputString += inChar;
    else  // /n - конец передачи
    {
      Serial.println ();
      Serial.println(inputString);
      Log.write ("Command:"+inputString);
      cmd = inputString.substring (0,4); // выделяем первые 4 символа
	  if (cmd=="read"){ 				 // команда на чтение файла из SD с начала файла
        readInProgress = true;      	 // флаг начала чтения файла
        current_pos = 0;
		titulString ();
      }
      else if (cmd=="last") {// команда на чтение из SD последних N записей файла
        s_num = inputString.substring (5); // выделяем число
		s_num.trim();
		if (isDigital(s_num)) {
			current_pos = setToLastRecord(s_num.toInt());
			readInProgress = true;           // флаг начала чтения файла
			titulString ();
		}
		else commandList ();
      }
      else if (cmd=="stop") {
        readInProgress = false;
        logReadInProgress = false;
      }
      else if (cmd=="blog") { // команда чтения log-файла с начала
				String buf;
				if (Log.getFirst (buf)) {
				Serial.print(buf);
			    logReadInProgress = true;           // флаг начала чтения файла
		    }
		    else {
		      logReadInProgress = false;
		    }
        }
      else if (cmd=="elog") { // команда чтения log-файла с конца
				String buf;
				s_num = inputString.substring (5); // выделяем число
				s_num.trim();
				if (isDigital(s_num)) {
					if (Log.getLast (buf,s_num.toInt())) {
						Serial.print(buf);
						logReadInProgress = true;           // флаг начала чтения файла
					}
				}
				else {
					logReadInProgress = false;
					commandList ();
				}
		}
      else if (cmd=="time") { // команда установки времени
				s_num = inputString.substring (5); // выделяем число
				s_num.trim();
				if (s_num.length()==13 && isDigital(s_num)) {
					writeDateTime(s_num);
					getDateDs1307();
					createRecord ();
					Serial.println (record);
				}
        }
      else if (cmd=="incr") { // команда корректировки времени в "+"
				s_num = inputString.substring (5); // выделяем число
				s_num.trim();
				if (isDigital(s_num)) {
					correctDateTime(s_num.toInt());
					getDateDs1307();
					createRecord ();
					Serial.println (record);
				}
        }
      else if (cmd=="decr") { // команда корректировки времени в "-"
				s_num = inputString.substring (5); // выделяем число
				s_num.trim();
				if (isDigital(s_num)) {
					correctDateTime(-(s_num.toInt()));
					getDateDs1307();
					createRecord ();
					Serial.println (record);
				}
        }
      else if ((cmd=="help") || (cmd=="?")) { // команда вывода списка команд
			commandList ();
        }
      else if (cmd=="dir") { // команда вывода списка команд
			root = SD.open("/");
			printDirectory(root, 0);
        }
      else if (cmd=="corr") {
        correctTime ();      // команда корректировки часов на основании вычисленной погрешности хода
		//tstTime_t ();
      }
      else  commandList ();
    inputString = "";
	}
  }
#endif //Serial
}
/*
* Вывод списка файлов.
*/
void printDirectory(File dir, int numTabs) 
{
#ifdef SERIAL
  while (true) {

    File entry =  dir.openNextFile();
    if (! entry) {
      // no more files
      break;
    }
    for (uint8_t i = 0; i < numTabs; i++) {
      Serial.print('\t');
    }
    Serial.print(entry.name());
    if (entry.isDirectory()) {
      Serial.println("/");
      printDirectory(entry, numTabs + 1);
    } else {
      // files have sizes, directories do not
      Serial.print("\t\t");
      Serial.println(entry.size(), DEC);
    }
    entry.close();
  }
#endif //Serial
}

/*
 * установка указателя чтения файла на n-ю запись с конца файла
 * int cnt - количество последних отображаемых записей
 */
unsigned long setToLastRecord(int cnt)
{
  unsigned long pos=0;
  int ch;
        // открыть файл 
        get_file_name();      // получить имя файла для текущего дня
        myFile = SD.open(filename);
        
      if (myFile) {
       for (pos=myFile.size()-2;pos!=0;pos--) {
        myFile.seek (pos);
        ch = myFile.peek ();
        if ((char)ch=='\x0A') {
          if (--cnt<=0 || ch == -1)   break;
        }
       }
        myFile.close();
      }
      return pos;
}

/*
* вывод списка команд
*/
void commandList ()
{
#ifdef SERIAL	
	Serial.println(F("dir  - read files list"));
	Serial.println(F("read  - read curent day file"));
	Serial.println(F("last nnn - read last nnn records curent day file"));
	Serial.println(F("blog  - read LOG-file"));
	Serial.println(F("elog nnn - read last nnn records LOG-file"));
	Serial.println(F("stop  - abort curent command"));
	Serial.println(F("time ddMMYYhhmmss0 - first setup time/date"));
	Serial.println(F("incr nnn - increment time to nnn sec"));
	Serial.println(F("decr nnn - decrement time to nnn sec"));
	Serial.println(F("help,?- list of available commands"));
#endif // SERIAL
}
/*
 * прочитать значения датчиков и сформировать строку для записи в SD
 */
void getDHT ()
{
int error;
char status;

createRecord (); //формирование времени для записи в SD

// получение с датчика данных влажности и температуры
error = dht.read();
lcd.setCursor(0,0);
lcd.display();
record +=";";
  switch(error){    // читаем показания датчика
    case DHT_OK:               record += String(dht.hum,2); break; 
    case DHT_ERROR_CHECKSUM:   lcd.print("ERROR_CHECKSUM"); record = "ERROR_CHECKSUM"; break;//HE PABEHCTBO KC
    case DHT_ERROR_DATA:       lcd.print("ERROR_DATA");     record = "ERROR_DATA";     break; //OTBET HE COOTBETCTB. CEHCOPAM 'DHT'
    case DHT_ERROR_NO_REPLY:   lcd.print("ERROR_NO_REPLY"); record = "ERROR_NO_REPLY"; break; //HET OTBETA
    default:                   lcd.print("ERROR");          record = "ERROR";          break;
  }

  status = pressure.startTemperature();
  if (status != 0)
  {
    // Wait for the measurement to complete:
    delay(status);

    // Retrieve the completed temperature measurement:
    // Note that the measurement is stored in the variable T.
    // Function returns 1 if successful, 0 if failure.

    status = pressure.getTemperature(T);
    if (status != 0)
    {
      // Print out the measurement:
		record = record + ";" + String(T,2);
    }
  }
  
  lcd.setCursor(0,1);
      status = pressure.startPressure(3);
      if (status != 0)
      {
        // Wait for the measurement to complete:
        delay(status);

        // Retrieve the completed pressure measurement:
        // Note that the measurement is stored in the variable P.
        // Note also that the function requires the previous temperature measurement (T).
        // (If temperature is stable, you can do one temperature measurement for a number of pressure measurements.)
        // Function returns 1 if successful, 0 if failure.

        status = pressure.getPressure(P,T);
        if (status != 0)
        {
          // Print out the measurement: P*0.0295333727 inHg, P*0.750063755419211 mmHg
          P = P*0.750063755419211;
          record = record + ";" + String(P,2);
        }
      }
      
      
  /*
    Exponential regression:
  GAS      | a      | b
  CO       | 605.18 | -3.937  
  Alcohol  | 77.255 | -3.18 
  CO2      | 110.47 | -2.862
  Tolueno  | 44.947 | -3.445
  NH4      | 102.2  | -2.473
  Acetona  | 34.668 | -3.369
  */
  MQ135.update(); // Update data, the arduino will be read the voltage on the analog pin

  MQ135.setA(605.18); MQ135.setB(-3.937); // Configurate the ecuation values to get CO concentration
  float CO = MQ135.readSensor(); // Sensor will read PPM concentration using the model and a and b values setted before or in the setup

  MQ135.setA(77.255); MQ135.setB(-3.18); // Configurate the ecuation values to get Alcohol concentration
  float Alcohol = MQ135.readSensor(); // Sensor will read PPM concentration using the model and a and b values setted before or in the setup

  MQ135.setA(110.47); MQ135.setB(-2.862); // Configurate the ecuation values to get CO2 concentration
  float CO2 = MQ135.readSensor(); // Sensor will read PPM concentration using the model and a and b values setted before or in the setup

  MQ135.setA(44.947); MQ135.setB(-3.445); // Configurate the ecuation values to get Tolueno concentration
  float Tolueno = MQ135.readSensor(); // Sensor will read PPM concentration using the model and a and b values setted before or in the setup

  MQ135.setA(102.2 ); MQ135.setB(-2.473); // Configurate the ecuation values to get NH4 concentration
  float Ammoniy = MQ135.readSensor(); // Sensor will read PPM concentration using the model and a and b values setted before or in the setup

  MQ135.setA(34.668); MQ135.setB(-3.369); // Configurate the ecuation values to get Acetona concentration
  float Aceton = MQ135.readSensor(); // Sensor will read PPM concentration using the model and a and b values setted before or in the setup

  record += ";" + String(CO,2);
  record += ";" + String(Alcohol,2);
  record += ";" + String(CO2,2);
  record += ";" + String(Tolueno,2);
  record += ";" + String(Ammoniy,2);
  record += ";" + String(Aceton,2);
  record += ";" + String(gasValue,DEC)+";";
      if (noGas) record += " no gas";
      else     record += " GAS !!!;";
#ifdef SERIAL
   if (!readInProgress) { // если не запущен процесс чтения файла из SD, то выводим в порт      
      Serial.println (record);
   }
#endif //Serial   
   
   showDHT (); // отобразить на LCD значения датчиков
}
/*
* построчное чтение log-файла
 * вызывается в цикле loop
*/
void readLog () 
{
#ifdef SERIAL	
  String buf;
  if (logReadInProgress) { // если запущен процесс чтения log-файла
		if (Log.getNext (buf)) {
			Serial.print(buf);
			logReadInProgress = true;
		}
		else logReadInProgress = false;
	}
#endif //SERIAL	
}
/*
 * побайтное чтение файла из SD
 * вызывается в цикле loop
 */
void readSD () 
{
      if (readInProgress) { // если запущен процесс чтения файла
        // открыть файл 
        get_file_name();      // получить имя файла для текущего дня
        myFile = SD.open(filename);
        
      if (myFile) {
        // чтение из файла 100 байт из current_pos и передача их в монитор порта
       for (int i=0;i<100;i++) { 
        if (myFile.seek (current_pos)) {
          Serial.write(myFile.read());
          current_pos++;
        }
        else {
          readInProgress = false; // нет доступных байтов
          Serial.println(F("END============"));
          break;
        }
       }
        // close the file:
        myFile.close();
      }
      else {
         Serial.print(F("error opening file:"));
         Serial.println(filename);
      }
     }
}

byte prev_minute;        // предыдущее значение минут для отсчета очередного интервала записи в SD
uint32_t  prev_calibr;   // отметка времени для отсчета интевала калибровки датчика газов
uint32_t time_display;   // отметка времени для включения/выключения дисплея
uint32_t time_delay;     // отметка времени для основного цикла
const uint32_t main_delay = 1000;  // задержка между чтениями часов
const uint32_t show_display = 65000; // время экспозиции дисплея
const byte cnt_expo = 5;      // количество основных циклов экспозиции на дисплее показаний датчиков
byte cnt_cicles;              // счетчик основных циклов в течение которых отображаются показания датчиков на дисплее
const uint32_t period_calibr=24*3600000;   // период калибровки датчика газов в часах
const uint32_t scroll_delay = 100;  // задержка скроллинга строки статуса
uint32_t scroll_millis;             // отметка времени для задержки скроллинга строки статуса

/*
 * программа калибровки датчика газов
 */
void MQ_calibration ()
{
  /*****************************  MQ CAlibration ********************************************/ 
  // Explanation: 
  // In this routine the sensor will measure the resistance of the sensor supposing before was pre-heated
  // and now is on clean air (Calibration conditions), and it will setup R0 value.
  // We recomend execute this routine only on setup or on the laboratory and save on the eeprom of your arduino
  // This routine not need to execute to every restart, you can load your R0 if you know the value
  // Acknowledgements: https://jayconsystems.com/blog/understanding-a-gas-sensor
  Serial.print(F("Calibrating please wait."));
  float calcR0 = 0;
  for(int i = 1; i<=10; i ++)
  {
    MQ135.update(); // Update data, the arduino will be read the voltage on the analog pin
    calcR0 += MQ135.calibrate(RatioMQ135CleanAir);
    Serial.print(".");
  }
  MQ135.setR0(calcR0/10);
  Serial.println(F("  done!."));
  prev_calibr = millis();     // новый отсчет интервала калибровки
  
  if(isinf(calcR0)) {Serial.println(F("Warning: Conection issue founded, R0 is infite (Open circuit detected) please check your wiring and supply"));}
  if(calcR0 == 0){Serial.println(F("Warning: Conection issue founded, R0 is zero (Analog pin with short circuit to ground) please check your wiring and supply"));}
  /*****************************  MQ CAlibration ********************************************/  
}
/*
* вывод строки заголовка в Serial для возможности получения CSV-файла
*/
void titulString ()
{
  Serial.println(F("Time;Humidity;Temp;Press;CO;Alcohol;CO2;Tolueno;Ammoniy;Aceton;integrally;Attention!;"));
}
/*
* добавить к бегущей статусной строке
*/
char statusString[LEN_STATUS_STRING];     //  буфер строки статуса
boolean statusStringAdd (char *newMessage)
{
	int lnStatusString, lnNewMessage, n;
	lnStatusString = strlen (statusString);
	lnNewMessage = strlen (newMessage);
	if (LEN_STATUS_STRING-lnStatusString-1 > lnNewMessage) {
		strcat (statusString, newMessage);
		return true;  // строка добавлена в буфер
	}
	return false;     // строка не помещатся в буфер
}
/*
* сдвиг строки статуса на один байт влево
*/
void statusStringShiftLeft ()
{
	memcpy (&statusString[0], &statusString[1], LEN_STATUS_STRING-2);
	statusString[LEN_STATUS_STRING-1] = 0;
}
/*
* отображение на TFT дата, время, датчики
*/
#define SYMBOL_WIDTH 6  // ширина символа на TFT в пикселах
int pixelShift=1;       // сдвиг символа на пикселы влево
void clock_128x64 ()
{
    // print time/date to 128x64
  char h_str[4];
  char m_str[4];
  char y_str[4];
  char out_str[30];
    out_str[0]=0;
    u8g2.clearBuffer();					// clear the internal memory once
    u8g2.setFontMode(0);
    u8g2.setFont(u8g2_font_profont12_tr);		  

	createRecord ();  // формирует строку reсord в виде hh:mm:ss
    record = "Time " + record;
    u8g2.setCursor(0,10);
    u8g2.print(record);   

    u8g2.setCursor(0,22);
    strcpy(h_str, u8x8_u8toa(dayOfMonth, 2));   // convert day to a string with two digits 
    strcpy(m_str, u8x8_u8toa(month, 2));		// convert month to a string with two digits
    strcpy(y_str, u8x8_u8toa(year, 2));		    // convert year to a string with two digits
	strcpy (out_str,"Date ");
	strcat(out_str,h_str);strcat(out_str,"/");
	strcat(out_str,m_str);strcat(out_str,"/");
	strcat(out_str,y_str);
    u8g2.print(out_str);   

    u8g2.setCursor(0,34);
    strcpy(h_str, u8x8_u8toa(dht.hum, 2));      // convert влажность to a string with two digits 
    strcpy(m_str, u8x8_u8toa(T, 2));		    // convert температура to a string with two digits
    //strcpy(y_str, u8x8_u8toa(P, 3));		    // convert давления to a string with two digits
	strcpy (out_str,"h:");
	strcat(out_str,h_str);strcat(out_str,"% t:");
	strcat(out_str,m_str);strcat(out_str," P:");
	u8g2.print(out_str);
	u8g2.setCursor(80,34);
	u8g2.print(P);	
	//strcat(out_str,y_str);strcat(out_str," mmHg");
    //u8g2.print(out_str); 
	
    u8g2.setCursor(0,46);
	strcpy (out_str,"gas:");
	u8g2.print("gas:");
	u8g2.setCursor(25,46);
	u8g2.print(gasValue);
	
	// формирование бегущей строки статуса внизу экрана
	pixelShift = --pixelShift % SYMBOL_WIDTH;  // смещение на 1 пиксел влево
    if (pixelShift == 0) //-(SYMBOL_WIDTH-1)) 
		statusStringShiftLeft ();              // смещение на символ влево
	u8g2.setCursor(pixelShift,58);
	strncpy (out_str, statusString, 23);
	u8g2.print(out_str);
	
    // make the result visible
    u8g2.sendBuffer(); // отображает буфер
}

void setup()
{
    // initialize the serial communications:
 Serial.begin(9600);
 Serial.print (F("period_calibr="));Serial.println (period_calibr);
  lcd.init();
  lcd.backlight(); // включение подсветки
// analogReference(INTERNAL);
 pressure.begin();

   // очистка строки статуса
   memset (&statusString[0],0,LEN_STATUS_STRING-1);
   strcpy (&statusString[0],"              start"); // видимая часть статуса (21 символ)

 
  if (!SD.begin(4)) { // выбор карты SD по пин 4
    Serial.println(F("initialization SD on pin 4 failed!"));
  }
  else Serial.println("initialization SD on pin 4 done.");

  getDateDs1307();//get the time data from tiny RTC

  Log.begin();
  Log.write (F("Setup"));

//  pinMode(digitalSignal, INPUT); //установка режима пина датчика газа
  //Set math model to calculate the PPM concentration and the value of constants
  MQ135.setRegressionMethod(1); //_PPM =  a*ratio^b
  
  /*****************************  MQ Init ********************************************/ 
  //Remarks: Configure the pin of arduino as input.
  /************************************************************************************/ 
  MQ135.init(); 
  /* 
    //If the RL value is different from 10K please assign your RL value with the following method:
    MQ135.setRL(10);
  */
  /*****************************  MQ CAlibration ********************************************/ 
  // MQ_calibration ();
  // Log.write ("MQ_calibration");
  
  prev_minute = minute; //сохраняем значение минут для цикла чтения датчиков
  time_display = millis();
  time_delay = millis();
  prev_calibr = millis()-period_calibr+300000; // установка следующей калибровки через 5 мин после запуска
  cnt_cicles = 0;
  heating = true;  //первичный нагрев датчика MQ135
  calibration = false; // true - если идет процесс калибровки

  filename[0] = 0;
  current_pos = 0;

  pinMode (A0,INPUT_PULLUP); // пин чтения мех.кнопки подтянут к питанию, нажатие кнопки садит на землю
  pinMode (2,INPUT); 		 // сенсорная кнопка
  pinMode (3,OUTPUT);        // управление подсветкой TFT
  digitalWrite(3, HIGH);     // включение подсветки TFT
  
  commandList ();			 // вывод списка команд
  titulString ();
  
  // init disp 128x64
  u8g2.begin();  
  u8g2.display();
  u8g2.setDisplayRotation(U8G2_MIRROR);  
  statusString[0] = 0;  
  
  // корректировка времени за интервал, прошедший с момента крайнего перезапуска программы
  correctTime ();

}

boolean isDHTshow=false; //если отображались датчики
void loop()
{
  int sensorValue;
  
  serialEvent(); // ожидание строки с командой или с данными даты и времени по Serial
  
  /* инициализация часов, если необходимо переустановить время */
  if (date_timeComplete && writeDateTime(inputString)){ 
    String rep = F("записано время в RTC ");
    rep += inputString;
    Serial.println (rep);
    Log.write (rep);
    inputString = "";
    date_timeComplete = false;
  }
  readSD ();  //чтение байта данных из SD, если из монитора порта поступала команда "read"
  readLog (); //чтение строки из log-файла, если из монитора порта поступала команда "log"
  
  /* управление подсветкой дисплея */
  if (abs(millis()-time_display) >= show_display) { // выключаем дисплей после 65с.
    lcd.noBacklight(); 
	digitalWrite(3, LOW);       // выключение подсветки TFT
	//u8g2.noDisplay();
  }
  sensorValue = analogRead(A0); // чтение нажатия мех.кнопки
  if (sensorValue <= 80) {      // если нажата кнопка - включаем дисплей
   time_display = millis();		// сохраняем тек.время включения дисплея
    lcd.backlight();            // включение подсветки
	digitalWrite(3, HIGH);      // включение подсветки TFT
	//u8g2.display();
  }
  sensorValue = digitalRead(2); // чтение сенсорной кнопки
  if (sensorValue) {      		// если нажата кнопка - включаем дисплей
   time_display = millis();		// сохраняем тек.время включения дисплея
    lcd.backlight();            // включение подсветки
	digitalWrite(3, HIGH);      // включение подсветки TFT
	//u8g2.display();
	showDHT (); 				// отобразить на LCD значения датчиков на 5 с
	isDHTshow = true;
	cnt_cicles = cnt_expo;		// период экспозиции показаний датчиков
  }
  
  /* отображение датчиков */
  if (prev_minute != minute)  {    // наступила следующая минута
     prev_minute = minute;  
     if (!isDHTshow) {
      lcd.clear(); // очистка после отображения времени
    }
    noGas = digitalRead(digitalSignal); //считываем значение о присутствии газа
    gasValue = analogRead(analogSignal); // и о его количестве
	
	if (heating && noGas) {
	   heating = false;   // окончание нагрева
	}

  /* калибровка датчика газа через заданный интевал, если не идет нагрев*/
  if (!heating && (abs(millis()-prev_calibr) >= period_calibr)) {
    calibration = true;
    MQ_calibration (); 
    Log.write (F("MQ_calibration"));
  }

    get_file_name(); // получить имя файла для текущего дня

    getDHT ();		// формирует строку с данными датчиков
    isDHTshow = true;
    
    // открыть файл или создать
    myFile = SD.open(filename, FILE_WRITE);
    if (myFile) {
      myFile.println(record);
      myFile.close();
    }
    else {
      Serial.print(F("error opening file:"));
      Serial.println(filename);
    }
    cnt_cicles = cnt_expo;
  }

  /* отображение времени на дисплее */
  if  (abs(millis()-time_delay) >= main_delay) {
    time_delay = millis();
    getDateDs1307();               //get the time data from tiny RTC
    
	if (cnt_cicles <= 0) { //количество циклов экспозиции показаний датчиков
      if (isDHTshow) lcd.clear();    // очистка после отображения датчиков
      isDHTshow=false;
      showDateTime ();
    }
    else cnt_cicles--;
	
	//вывод сообщений в строку статуса на TFT 128x64
	if (calibration) {
		// пытаемся добавить сообщение в статус-строку пока не добавится
		if (statusStringAdd (" calibration..")) calibration=false;
	}
    else if (heating) statusStringAdd (" heating MQ135 ...");
    else if (noGas)   statusStringAdd ("  gas in norme...");    
	else              statusStringAdd ("  !!! GAS !!!  ");	
  }  

  if  (abs(millis()-scroll_millis) >= scroll_delay) {	
	scroll_millis = millis();
	clock_128x64 (); // print time, date, DHT to TFT 128x64
  }
}
