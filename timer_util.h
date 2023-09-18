#pragma once
#define DS1307_I2C_ADDRESS 0x68 // the I2C address of Tiny RTC
byte second, minute, hour, dayOfWeek, dayOfMonth, month, year;
// структура для сохранения в EEPROM данных юстировок и коррекций RTCчасов
// юстировка - выполнение команды компьютера "time...", принудительная установка и вычисление погрешности часов
// коррекция - текущая подстройка RTC-часов без участия компьютера на основе вычисленной при юстировке погрешности 
// коррекция может вызываться при перезапуске в функции setup, или по команде "corr"
struct dt_EEPROM {
	byte second;           // дата и время корректировки функцией correctTime ()
	byte minute;
	byte hour;
	byte dayOfMonth;
	byte month;
	byte year;
	uint32_t interval;   // интервал времени, необходимый для корекции на 1 сек
	int correction;      // знак коррекции на 1 сек RTC-часов за interval
	uint32_t total_sec;  // нарастающий итог секунд, на которые были скорректированы RTC-часы за все корректировки
	time_t prev_just;    // дата и время предыдущей юстировки RTC-часов по команде компьютера "time..."
};
dt_EEPROM to_EEPROM, from_EEPROM; // для записи в EEPROM и для чтения из EEPROM
time_t time_tRTC,time_tReal;      // для вычисления разности в сек двух дат
tm tmRTC,tmReal;                  // для формирования двух дат

// Convert normal decimal numbers to binary coded decimal
byte decToBcd(byte val)
{
  return ( (val / 10 * 16) + (val % 10) );
}
// Convert binary coded decimal to normal decimal numbers
byte bcdToDec(byte val)
{
  return ( (val / 16 * 10) + (val % 16) );
}

// Function to set the current time, change the second&minute&hour to the right time
void setDateDs1307()
{
  Wire.beginTransmission(DS1307_I2C_ADDRESS);
  Wire.write(decToBcd(0));
  Wire.write(decToBcd(second)); // 0 to bit 7 starts the clock
  Wire.write(decToBcd(minute));
  Wire.write(decToBcd(hour)); // If you want 12 hour am/pm you need to set
  Wire.write(decToBcd(dayOfWeek));
  Wire.write(decToBcd(dayOfMonth));
  Wire.write(decToBcd(month));
  Wire.write(decToBcd(year));
  Wire.endTransmission();
}
// Function to gets the date and time from the ds1307 and prints result
void getDateDs1307()
{
  // Reset the register pointer
  Wire.beginTransmission(DS1307_I2C_ADDRESS);
  Wire.write(decToBcd(0));
  Wire.endTransmission();
  Wire.requestFrom(DS1307_I2C_ADDRESS, 7);
  second = bcdToDec(Wire.read() & 0x7f);
  minute = bcdToDec(Wire.read());
  hour = bcdToDec(Wire.read() & 0x3f); // Need to change this if 12 hour am/pm
  dayOfWeek = bcdToDec(Wire.read());
  dayOfMonth = bcdToDec(Wire.read());
  month = bcdToDec(Wire.read());
  year = bcdToDec(Wire.read());
}
/*
 * корректирует время на количество секунд, если получена команда корректировки времени
 */
void correctDateTime(int sec)
{
    getDateDs1307(); // прочитать время из RTC
	
	second += sec%60;
	minute += sec/60;
    setDateDs1307(); // записать время в RTC
    
    return;
}
bool isEEPROMcorrect () {
	if (from_EEPROM.second<60 and 
	       from_EEPROM.minute <60 and 
		   from_EEPROM.hour <24 and 
		   from_EEPROM.dayOfMonth <31 and
		   from_EEPROM.month <13 and
		   from_EEPROM.year >22)
        return true;
	else
		return false;

}
// тест функций времени
void tstTime_t ()
{
	    String str;

        str = "localtime:";
		str = str +" yy=" + String (year);
		str = str +" MM=" + String (month); 
		str = str +" dd=" + String (dayOfMonth);
		str = str +" hh=" + String (hour);
		str = str +" mm=" + String (minute);
		str = str +" ss=" + String (second);
		Serial.println (str);

	// сохраняем текущее время из RTC
	tmRTC.tm_sec  = second;  
	tmRTC.tm_min  = minute;  
	tmRTC.tm_hour = hour; 
	tmRTC.tm_mday = dayOfMonth; 
	tmRTC.tm_mon  = month;  
	tmRTC.tm_year = year+100; 

	time_tRTC  = mktime (&tmRTC) + 10;
		// формируем текущее время
		tm *ltime; 
		ltime = localtime (&time_tRTC);
		
        str = "corrected localtime +10:";
		str = str +" yy=" + String(ltime->tm_year-100);
		str = str +" MM=" + String (ltime->tm_mon); 
		str = str +" dd=" + String(ltime->tm_mday);
		str = str +" hh=" + String (ltime->tm_hour);
		str = str +" mm=" + String (ltime->tm_min);
		str = str +" ss=" + String (ltime->tm_sec);
		Serial.println (str);
	
}
/*
 * записывает дату и время в модуль часов, если получена из компа команда "time" установки даты/времени
 * вычисляет данные для последующей коррекции RTC-часов
 */
boolean writeDateTime(String inputString)
{
    String str;
	getDateDs1307();  // вычитываем время из RTC
	
	// сохраняем текущее время из RTC
	tmRTC.tm_sec  = second;  
	tmRTC.tm_min  = minute;  
	tmRTC.tm_hour = hour; 
	tmRTC.tm_mday = dayOfMonth; 
	tmRTC.tm_mon  = month;  
	tmRTC.tm_year = year+100; 
	// сохраняем время, полученое из компа, и передаем его в RTC
	dayOfMonth = (int(inputString[0]) - 48) * 10 + (int(inputString[1]) - 48);
    month = (int(inputString[2]) - 48) * 10 + (int(inputString[3]) - 48);
    year = (int(inputString[4]) - 48) * 10 + (int(inputString[5]) - 48);
    hour = (int(inputString[6]) - 48) * 10 + (int(inputString[7]) - 48);
    minute = (int(inputString[8]) - 48) * 10 + (int(inputString[9]) - 48);
    second = (int(inputString[10]) - 48) * 10 + (int(inputString[11]) - 48);
    dayOfWeek = (int(inputString[12]) - 48) * 10;
    
	setDateDs1307(); // записать время в RTC
    
	// сохраняем время, полученое из компа, для вычисления погрешности
	tmReal.tm_sec  = second;
	tmReal.tm_min  = minute;
	tmReal.tm_hour = hour;
	tmReal.tm_mday = dayOfMonth;
	tmReal.tm_mon  = month;
	tmReal.tm_year = year+100;  // +100 ???
	// вычисляем разницу в секундах и сохраняем в структуре для записи в EEPROM
	time_tReal = mktime (&tmReal);
	time_tRTC  = mktime (&tmRTC);
	to_EEPROM.correction  = difftime (time_tReal, time_tRTC);
	str = "time_tReal =" + String (time_tReal);
	str = str + "time_tRTC =" + String (time_tRTC);
	str = str + "calculate correction:"+ String (to_EEPROM.correction);
	Serial.println (str);
	// вычитываем из EEPROM данные о предыдущей юстировке
	EEPROM.get(0, from_EEPROM);   // прочитать из адреса 0
    str = "from EEPROM:";
	str = str +" yy=" + String (from_EEPROM.year);
	str = str +" MM=" + String (from_EEPROM.month); 
	str = str +" dd=" + String (from_EEPROM.dayOfMonth);
	str = str +" hh=" + String (from_EEPROM.hour);
	str = str +" mm=" + String (from_EEPROM.minute);
	str = str +" ss=" + String (from_EEPROM.second);
	str = str +" cor="+ String (from_EEPROM.correction);
	str = str +" int="+ String (from_EEPROM.interval);
	str = str +" total_sec="+ String (from_EEPROM.total_sec);
	str = str +" prev_just="+ String (from_EEPROM.prev_just);
	Serial.println (str);
	if (to_EEPROM.correction > -2 and to_EEPROM.correction < 2) {
		Serial.println ("No colibration");
		return false; // не обновляем запись в EEPROM
	}
    to_EEPROM.second      = second;
    to_EEPROM.minute      = minute;
    to_EEPROM.hour        = hour;
    to_EEPROM.dayOfMonth  = dayOfMonth;
    to_EEPROM.month       = month;
    to_EEPROM.year        = year;
	to_EEPROM.prev_just   = time_tReal; // новая точка отсчета
	to_EEPROM.total_sec   = 0;
	if (isEEPROMcorrect ()) {
		// дата и время последней юстировки вроде бы корректные
		tmRTC.tm_sec  = from_EEPROM.second;
		tmRTC.tm_min  = from_EEPROM.minute;
		tmRTC.tm_hour = from_EEPROM.hour;
		tmRTC.tm_mday = from_EEPROM.dayOfMonth;
		tmRTC.tm_mon  = from_EEPROM.month;
		tmRTC.tm_year = from_EEPROM.year+100;
		// вычисляем интервал между юстировками в секундах, за который набегает погрешность 1 сек.
		// time_tRTC  = mktime (&tmRTC);
		// to_EEPROM.interval = difftime (time_tReal, time_tRTC);
		to_EEPROM.interval = difftime (time_tReal, from_EEPROM.prev_just);
		str = "time_tReal=" + String(time_tReal);
		str = str + " prev_just=" + from_EEPROM.prev_just;
		str = str + " interval = difftime (time_tReal, prev_just)=" + String (to_EEPROM.interval);
		Serial.println (str);
		// вычисляем интервал, за который набегает погрешность 1 сек. с учетом всех прошедших коррекций
	    to_EEPROM.interval = to_EEPROM.interval / (abs(to_EEPROM.correction)+from_EEPROM.total_sec);
		str = "total sec to correction=" + String (abs(to_EEPROM.correction)+from_EEPROM.total_sec);
		str = str + " just interval pro sec=" + String (to_EEPROM.interval);
		Serial.println (str);
		// сохраняем знак коррективки
		if (to_EEPROM.correction > 0)      to_EEPROM.correction = 1;
		else if (to_EEPROM.correction < 0) to_EEPROM.correction = -1;
		else     {to_EEPROM.correction = 0;to_EEPROM.interval = 0;} // за данный интервал разницы часов нет
		}
	else {
			// в EEPROM билиберда  - устанавливаем интервал 0
			to_EEPROM.interval = 0;
			to_EEPROM.correction = 0;
		}
	to_EEPROM.total_sec = 0;     // новая точка отсчета
	to_EEPROM.prev_just = time_tReal; 
	// зписываем в EEPROM
    str = "to_EEPROM:";
	str = str +" yy=" + String (to_EEPROM.year);
	str = str +" MM=" + String (to_EEPROM.month);
	str = str +" dd=" + String (to_EEPROM.dayOfMonth);
	str = str +" hh=" + String (to_EEPROM.hour);
	str = str +" mm=" + String (to_EEPROM.minute);
	str = str +" ss=" + String (to_EEPROM.second);
	str = str +" cor="+ String (to_EEPROM.correction);
	str = str +" int="+ String (to_EEPROM.interval);
	str = str +" total_sec="+ String (to_EEPROM.total_sec);
	str = str +" prev_just="+ String (to_EEPROM.prev_just);
	Serial.println (str);
	
	EEPROM.put(0, to_EEPROM);     // поместить в EEPROM по адресу 0
    
	return true;
}
/*
* коррекция времени без участия компьютера по записанным в EEPROM данным о погрешности часов
*/
void correctTime ()
{
  	String str;
	EEPROM.get(0, from_EEPROM);   // прочитать из адреса 0 данные предыдущей коррекции
    str = "from EEPROM:";
	str = str+" yy=" + String(from_EEPROM.year);
	str = str+" MM=" + String (from_EEPROM.month); 
	str = str+" dd=" + String(from_EEPROM.dayOfMonth);
	str = str+" hh=" + String (from_EEPROM.hour);
	str = str+" mm=" + String (from_EEPROM.minute);
	str = str+" ss=" + String (from_EEPROM.second);
	str = str+" cor=" + String (from_EEPROM.correction);
	str = str+" int=" + String (from_EEPROM.interval);
	str = str +" total_sec="+ String (from_EEPROM.total_sec);
	str = str +" prev_just="+ String (from_EEPROM.prev_just);
	Serial.println (str);
	if (from_EEPROM.correction==0 or from_EEPROM.interval==0) {
	   Serial.println ("No update date-time");
	   return; // не обновляем время
	}
	if (!isEEPROMcorrect ()) {
	   Serial.println ("No EEPROM correct");
	   return; // не обновляем время
	}
		// вычисляем интервал между текущей датой и датой прошлой корректировки в секундах
		tmRTC.tm_sec  = from_EEPROM.second;
		tmRTC.tm_min  = from_EEPROM.minute;
		tmRTC.tm_hour = from_EEPROM.hour;
		tmRTC.tm_mday = from_EEPROM.dayOfMonth;
		tmRTC.tm_mon  = from_EEPROM.month;
		tmRTC.tm_year = from_EEPROM.year+100;
		
		tmReal.tm_sec  = second;
		tmReal.tm_min  = minute;
		tmReal.tm_hour = hour;
		tmReal.tm_mday = dayOfMonth;
		tmReal.tm_mon  = month;
		tmReal.tm_year = year+100; // +100 ??????????????
		
		time_tReal = mktime (&tmReal);
		time_tRTC  = mktime (&tmRTC);
		uint32_t interval  = difftime (time_tReal, time_tRTC);
		str = "time_tReal=" + time_tReal;
		str = str + " time_tRTC=" + time_tRTC;
		str = str + "interval between correction=" + String (interval);
		Serial.println (str);
	    // вычисляем количество интервалов, прошедших с момента прошлой корректировки
		// это будет количество секунд, на которое надо скорректировать RTC-часы
		int corr_sec=0;
		if (from_EEPROM.interval != 0)
            corr_sec = interval / from_EEPROM.interval;
		str = "count intervals =" + String (corr_sec);
		str = str + " corr_sec =" + String (from_EEPROM.correction * corr_sec);
		Serial.println (str);
		// корректируем текущее время на количество секунд с учетом знака коррекции
		time_tReal += from_EEPROM.correction * corr_sec;
		// формируем текущее время
		tm *ltime; 
		ltime = localtime (&time_tReal);
		char s_time[40];
		strftime (s_time, 39,"%X %d-%m-%Y", ltime);
		Serial.println (s_time);
		second    = ltime->tm_sec;
		minute    = ltime->tm_min; 
		hour      = ltime->tm_hour;
		dayOfMonth= ltime->tm_mday;
		month     = ltime->tm_mon; 
		year      = ltime->tm_year-100;
		
        str = "corrected localtime:";
		str = str +" yy=" + String(year);
		str = str +" MM=" + String (month); 
		str = str +" dd=" + String(dayOfMonth);
		str = str +" hh=" + String (hour);
		str = str +" mm=" + String (minute);
		str = str +" ss=" + String (second);
		Serial.println (str);
	
	    if (corr_sec != 0) {
			setDateDs1307(); // записать время в RTC
			
			// записываем дату текущей корректировки в EEPROM, не меняя интервал
    		to_EEPROM.second      = second;
    		to_EEPROM.minute      = minute;
    		to_EEPROM.hour        = hour;
    		to_EEPROM.dayOfMonth  = dayOfMonth;
    		to_EEPROM.month       = month;
    		to_EEPROM.year        = year;
			to_EEPROM.interval    = from_EEPROM.interval;
			to_EEPROM.correction  = from_EEPROM.correction;
			to_EEPROM.total_sec   = from_EEPROM.total_sec + corr_sec;
	        // зписываем в EEPROM
	        EEPROM.put(0, to_EEPROM);     // поместить в EEPROM по адресу 0
		}
}
