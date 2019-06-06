#ifndef _TCP_SERVER_H_
#define _TCP_SERVER_H_

#include "Selector.hpp"
#include "Timer.hpp"
#include <queue>
#include <thread>
#include <mutex>
#include <atomic>
#include <condition_variable>

class TcpStream;

class NetMsgTask
{
public:
	NetMsgTask(int io_thread_id = -1, NetID id = -1);
	~NetMsgTask() = default;

	bool DoWork();

private:
	int	  _io_thread_id;
	NetID _id;
};

class NetMsgThread
{
public:
	NetMsgThread(int work_thread_id = -1);

	~NetMsgThread();

	void StartUp();
	void Stop();
	void Run();
	bool Push(NetMsgTask* task);
	NetMsgTask* Pop();

private:
	std::mutex      _mutex;
	std::condition_variable	_lock_cond;
	std::queue<NetMsgTask*> _task_queue;  //任务队列
	std::thread *	_work_thread;
	int				_work_thread_id;
	bool			_IsStart;
};

class NetIOThread
{
public:
	NetIOThread(int io_thread_id = -1);
	~NetIOThread();

	void StartUp();	
	void Stop();
	bool UpdataClientList();
	void Run();	

	void AddClient(TcpStream* client);
	void RemoveClient(TcpStream* client);
	size_t GetClientNum();

private:
	Selector<TcpStream>				_selctor;
	std::queue<TcpStream*>			_new_client_queue;
	std::queue<TcpStream*>			_offline_client_queue;
	std::thread *					_work_thread;
	bool							_IsStart;
	std::condition_variable			_lock_cond;
	std::mutex						_lock;
	int								_io_thread_id;
	int								_client_num;
	Timer							_timer;
};

#endif