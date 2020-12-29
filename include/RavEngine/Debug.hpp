#pragma once
#include "SpinLock.hpp"
#include <fmt/format.h>
#include <chrono>

namespace RavEngine {

class Debug{
private:
	Debug() = delete;
	static SpinLock mtx;
	
	/**
	 @return  the current time as a string
	 */
	inline static const char* DateString(){
		time_t _tm = time(NULL);
		struct tm * curtime = localtime ( &_tm );
		char* str = asctime(curtime);
		str[strlen(str)-1] = '\0';	//remove newline
		return str;
	}
	
	static inline void LogHelper(FILE* output, const char* message, const char* type){
		mtx.lock();
		fprintf(output,"[%s] %s - %s\n",DateString(),type,message);
		mtx.unlock();
	}
	
public:
	
	/**
	 Log a message to standard output. In release builds, this method is stubbed and logs are disabled.
	 @param message The message to log
	 */
	static inline void LogTemp(const char* message){
#ifdef _DEBUG
		LogHelper(stdout, message, "LOGTEMP");
#endif
	}
	
	/**
	 Log a message to standard output. In release builds, this method is stubbed and logs are disabled.
	 @param formatstr The formatting string to log
	 @param values the optional values to log
	 */
	template <typename ... T>
	static inline void LogTemp(const char* formatstr, T... values){
#ifdef _DEBUG
		LogHelper(stdout,fmt::format(formatstr,values...).c_str(),"LOGTEMP");
#endif
	}
	
	/**
	 Log a message to standard output.
	 @param message The message to log
	 */
	static inline void Log(const char* message){
		LogHelper(stdout, message, "LOG");
	}
	
	/**
	 Log a message to standard output.
	 @param formatstr The formatting string to log
	 @param values the optional values to log
	 */
	template <typename ... T>
	static inline void Log(const char* formatstr, T... values){
		LogHelper(stdout,fmt::format(formatstr,values...).c_str(),"LOG");
	}
	
	/**
	 Log a message to standard error, as a warning.
	 @param message The message to log.
	 */
	static inline void Warning(const char* message){
		LogHelper(stderr, message, "WARN");
	}
	
	/**
	 Log a message to standard error, as a warning.
	 @param formatstr The formatting string to log
	 @param values the optional values to log
	 */
	template <typename ... T>
	static inline void Warning(const char* formatstr, T... values){
		LogHelper(stderr, fmt::format(formatstr,values...).c_str(), "WARN");
	}
	
	/**
	 Log a message to standard error, as an error.
	 @param message The message to log.
	 */
	static inline void Error(const char* message){
		LogHelper(stderr, message, "ERROR");
	}
	
	/**
	 Log a message to standard error, as an error.
	@param formatstr The formatting string to log
	@param values the optional values to log
	*/
	template <typename ... T>
	static inline void Error(const char* formatstr, T... values){
		LogHelper(stderr, fmt::format(formatstr,values...).c_str(), "ERROR");
	}
	
	/**
	 Log an error message and terminate.
	 @param message The message to log.
	 */
	static inline void Fatal(const char* message){
		throw std::runtime_error(message);
	}
	
	/**
	 Log an error message and terminate.
	 @param formatstr The formatting string
	 @param values the optional values
	 */
	template <typename ... T>
	static inline void Fatal(const char* formatstr, T... values){
		throw std::runtime_error(fmt::format(formatstr,values...));
	}
};
	
}
