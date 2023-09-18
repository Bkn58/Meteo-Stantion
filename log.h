/*
* класс работы с LOG-файлом
* использует дату и время из глобальных переменных RTC, 
* поэтому подключается после библиотеки "Timer_util.h"
*/

#pragma once
// подключение библиотеки для SD
#include <SD.h>

class log_file {
	public:
	int begin ();				// вызывать только после инициализации SD-карты в функции setup {} !!!!!
	int write (String record);	// запись в конец LOG-файла
	int getFirst (String& outStr);// чтение первой записи
	int getNext (String& outStr); // чтение следующей записи
	int getLast (String& outStr,unsigned long n); // чтение n-ой с конца записи
	
	private:
	String _buf;				// буфер для читаемой записи
	char _fName[14];			// имя LOG-файла
	File _logFile;				// дескриптор файла
	unsigned long _pointer;		// указатель на запись
	unsigned long setToLast (int n); // установка на начало n-ой записи с конца
};

int log_file::begin () {
	strcpy(_fName,"log.txt");
	_logFile = SD.open (_fName, FILE_WRITE);
	if (_logFile) {_logFile.close(); return 1;}
	else         return 0;
}

int log_file::write (String record) {
	_logFile = SD.open (_fName, FILE_WRITE);
	if (_logFile) {
		if (dayOfMonth<10) _logFile.print ("0");
		_logFile.print (dayOfMonth); _logFile.print ("-");
		if (month<10) _logFile.print ("0");
		_logFile.print (month); _logFile.print ("-");
		_logFile.print (year); _logFile.print (" ");
		if (hour<10)  _logFile.print ("0");
		_logFile.print (hour); _logFile.print (":");
		if (minute<10) _logFile.print ("0");
		_logFile.print (minute); _logFile.print (":");
		if (second<10) _logFile.print ("0");
		_logFile.print (second); _logFile.print ("=>");
		_logFile.println(record);
		_logFile.flush();
		_logFile.close();
		return 1;
	}
	else return 0;
}

/*
* чтение первой записи
*/
int log_file::getFirst (String& outStr) {
  int ch;
  int ret = 1;
		
		_buf = "";
		_pointer = 0;
        // открыть файл 
        _logFile = SD.open(_fName);
       
		if (_logFile) {
			for (_pointer=0;_pointer<_logFile.size();_pointer++) {
				ch = _logFile.read();
				_buf += (char)ch;
				if ((char)ch=='\x0A') {_pointer++; break;} // конец строки
				if (ch == -1) {ret = 0; _pointer++;break;} // конец файла
			}
		_logFile.close();
		}
	outStr = _buf;
	return ret;
}
/*
* чтение следующей записи
*/
int log_file::getNext (String& outStr){
  int ch;
  int ret = 1;
		_buf = "";
        // открыть файл 
        _logFile = SD.open(_fName);
        
		if (_logFile) {
			for (;_pointer<=_logFile.size();_pointer++) {
				if (_logFile.seek(_pointer)) {
					ch = _logFile.peek();
					_buf += (char)ch;
					if ((char)ch=='\x0A') {_pointer++; break;} // конец строки
					if (ch == -1) {ret = 0; _pointer++;break;} // конец файла
				}
				else {ret = 0;} // конец файла
			}
		_logFile.close();
		//Serial.print (_buf);
		}
	outStr = _buf;
	return ret;
}	
/*
* установка на начало последней записи
* parametr cnt - номер записи с конца
*/
unsigned long log_file::setToLast (int cnt){
  int ch;
		
		_pointer = 0;
        // открыть файл 
        _logFile = SD.open(_fName);
        
      if (_logFile) {
       for (_pointer=_logFile.size()-2;_pointer!=0;_pointer--) {
        _logFile.seek (_pointer);
        ch = _logFile.peek ();
        if ((char)ch=='\x0A') {
          if (--cnt<=0 || ch == -1)   break;
        }
       }
        _logFile.close();
      }

      return _pointer;
	
}
/*
* чтение n-ой с конца записи
*/
int log_file::getLast (String& outStr,unsigned long n){
  int ch;
  int ret = 1;
		_buf = "";
		setToLast (n);
        // открыть файл 
        _logFile = SD.open(_fName);
		if (_logFile) {
			_logFile.seek(_pointer);
			for (;_pointer<_logFile.size();_pointer++) {
				ch = _logFile.read();
				_buf += (char)ch;
				if ((char)ch=='\x0A') {_pointer++; break;} // конец строки
				if (ch == -1) {ret = 0; _pointer++;break;} // конец файла
			}
      _logFile.close();
      //Serial.print (_buf);
		}
	outStr = _buf;
	return ret;
}

extern log_file Log;
log_file Log = log_file ();
