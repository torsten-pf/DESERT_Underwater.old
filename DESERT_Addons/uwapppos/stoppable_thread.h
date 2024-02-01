/**
 * @file   stoppable_thread.h
 * @author Torsten Pfuetzenreuter
 * @version 1.0.0
 *
 * \brief Provides a simple C++11 thread the StoppableThread class
 *
 */

#pragma once

#include <atomic>
#include <thread>
using namespace std::chrono_literals;

class StoppableThread
{
public:
	bool Start()
	{
		try
		{
			m_thread = std::thread(&StoppableThread::RunInternal, this);
			return true;
		}
		catch (...)
		{
			return false;
		}
	}
	void Stop(bool wait = false)
	{
		m_stop.store(true);
		if (wait && m_thread.joinable())
		{
			try
			{
				m_thread.join();
			}
			catch (...)
			{
			}
		}
	}
	virtual void Run()
	{
		while (!StopRequested())
		{
			Sleep(100ms);
		}
	}
	/** Sleep for the given duration, use literals like 1s, 100ms, 10us */
	template <class Rep, class Period>
	void Sleep(const std::chrono::duration<Rep, Period> &d)
	{
		std::this_thread::sleep_for(d);
	}
	bool Running() { return m_running.load(); }
	bool StopRequested() { return m_stop.load(); }

private:
	void RunInternal()
	{
		m_running = true;
		Run();
		m_running = false;
	}
	std::atomic_bool m_running{false};
	std::atomic_bool m_stop{false};
	std::thread m_thread;
};
