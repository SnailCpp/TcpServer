#ifndef _TIMER_HPP_
#define _TIMER_HPP_

#include <chrono>
class Timer
{
public:
	Timer()
	{
		_start_time_point = std::chrono::high_resolution_clock::now();
	}

	~Timer()
	{

	}

	void Updata()
	{
		_start_time_point = std::chrono::high_resolution_clock::now();
	}

	// 获取当前秒数
	double GetElapsedSecond()
	{
		return this->GetElapsedMicroSecond() * 0.000001;
	}

	// 获取当前毫秒数
	double GetElapsedMilliSecond()
	{
		return this->GetElapsedMicroSecond() * 0.001;
	}

	long long GetElapsedMicroSecond()
	{		
		return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - _start_time_point).count();
	}

private:
	std::chrono::time_point<std::chrono::high_resolution_clock> _start_time_point;
};

#ifdef _WIN32
#include <windows.h>
class WindowsTimer
{	
public:
	WindowsTimer()
	{
		QueryPerformanceFrequency(&_performance_frequency);
		QueryPerformanceCounter(&_performance_count_start);
	}

	~WindowsTimer()
	{
	}

	void Updata()
	{
		QueryPerformanceCounter(&_performance_count_start);
	}

	// 获取当前秒数
	double GetElapsedSecond()
	{
		return this->GetElapsedMicroSecond() * 0.000001;
	}

	// 获取当前毫秒数
	double GetElapsedMilliSecond()
	{
		return this->GetElapsedMicroSecond() * 0.001;
	}

	double GetElapsedMicroSecond()
	{
		QueryPerformanceCounter(&_performance_count_end);
		double start_micro_second = _performance_count_start.QuadPart * ((1.0 * 1000 * 1000) / _performance_frequency.QuadPart);
		double end_micro_second = _performance_count_end.QuadPart * ((1.0 * 1000 * 1000) / _performance_frequency.QuadPart);
		return end_micro_second - start_micro_second;
	}

private:
	LARGE_INTEGER  _performance_frequency;
	LARGE_INTEGER  _performance_count_start, _performance_count_end;;
};

#endif

#endif // _TIMER_HPP_