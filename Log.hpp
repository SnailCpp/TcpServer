
#ifndef _LOG_HPP_
#define _LOG_HPP_

#include <string>
#include <stdarg.h>
#include <time.h>

class Log
{
public:
	enum LogLevel
	{
		DebugLevel,
		InfoLevel,
		ErrorLevel,
	};

	static void Printf(LogLevel level, const char *fmt, ...)
	{
		va_list va_ptr;
		va_start(va_ptr, fmt);
		static char info_buf[1024];
		memset(info_buf, '\0', sizeof(info_buf));
		vsnprintf(info_buf, sizeof(info_buf), fmt, va_ptr);	
		
		static char time_buf[32];
		memset(time_buf, '\0', sizeof(time_buf));	
		time_t t = time(NULL);
		tm* local = localtime(&t); 
		strftime(time_buf, sizeof(time_buf), "[%Y-%m-%d %H:%M:%S] ", local);		

		switch (level)
		{	
		case DebugLevel: fprintf(stdout, (time_buf + std::string("[DEDUG]\t")  + info_buf).c_str()); break;
		case InfoLevel:  fprintf(stdout, (time_buf + std::string("[INFO]\t") + info_buf).c_str()); break;
		case ErrorLevel: fprintf(stderr, (time_buf + std::string("[ERROR]\t") + info_buf).c_str()); break;
		default: break;
		}

		va_end(va_ptr);
	}

	template <typename ...T>
	static void Debug(const char *fmt, T& ...args)
	{		
		Printf(DebugLevel, fmt, args...);
	}

	template <typename ...T>
	static void Info(const char *fmt, T& ...args)
	{		
		Printf(InfoLevel, fmt, args...);
	}

	template <typename ...T>
	static void Error(const char *fmt, T& ...args)
	{		
		Printf(ErrorLevel, fmt, args...);
	}
};

#endif