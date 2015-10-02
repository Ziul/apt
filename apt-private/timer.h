#ifndef TIMER__H__
#define TIMER__H__

#include <chrono>

using namespace std::chrono;

class timer
{
private:
	std::chrono::time_point<std::chrono::system_clock> start, stop;
	std::chrono::duration<double> elapsed_seconds;
public:
	timer(){}
	~timer(){}

	void begin()
	{
		this->start = system_clock::now();	
	}

	double end()
	{
		this->stop = std::chrono::system_clock::now();
		this->elapsed_seconds = this->stop- this->start;
		return elapsed_seconds.count();
	}


	double currenttime()
	{
		return elapsed_seconds.count();
	}
};


#endif