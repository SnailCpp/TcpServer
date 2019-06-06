#include "TcpNetTask.h"
#include "TcpNetMgr.h"
#include "TcpNetwork.hpp"
#include "Message.hpp"
#include "Timer.hpp"
#include "Buffer.hpp"
#include <stdio.h>
#include <vector>
#include <list>
#include <queue>
#include <map>

NetMsgTask::NetMsgTask(int io_thread_id, NetID id) 
	:_io_thread_id(io_thread_id), _id(id)
{

}

bool NetMsgTask::DoWork()
{
	return TcpNetMgr::Instance().OnNetMsg(_io_thread_id, _id);
}

NetMsgThread::NetMsgThread(int work_thread_id) 
	:_work_thread(nullptr), _work_thread_id(work_thread_id), _IsStart(false)
{
	this->StartUp();
}

NetMsgThread::~NetMsgThread()
{
	this->Stop();
}

void NetMsgThread::StartUp()
{
	if (nullptr == _work_thread)
	{
		_IsStart = true;
		_work_thread = new std::thread(&NetMsgThread::Run, this);
	}
}

void NetMsgThread::Stop()
{

	if (_work_thread)
	{
		_work_thread->join();
		_work_thread = nullptr;
		_IsStart = false;
	}
}

void NetMsgThread::Run()
{
	while (_IsStart)
	{
		std::unique_lock<std::mutex> uni_lock(_mutex);
		while (_task_queue.empty())
		{
			_lock_cond.wait(uni_lock);
		}
		if (uni_lock.owns_lock()) uni_lock.unlock();

		NetMsgTask* it = Pop();
		if (nullptr != it)
		{
			if (it->DoWork()) delete it;
		}	
	}
}

bool NetMsgThread::Push(NetMsgTask* task)
{
	if (task == nullptr)
	{
		return false;
	}

	_mutex.lock();
	_task_queue.push(task);
	_mutex.unlock();
	_lock_cond.notify_one();

	return true;
}

NetMsgTask* NetMsgThread::Pop()
{
	NetMsgTask* it = nullptr;

	_mutex.lock();
	if (!_task_queue.empty())
	{
		it = _task_queue.front();
		_task_queue.pop();
	}
	_mutex.unlock();
	return it;
}

NetIOThread::NetIOThread(int io_thread_id)
	:_work_thread(nullptr), _IsStart(false), _io_thread_id(io_thread_id), _client_num(0)
{
	this->StartUp();
}

NetIOThread::~NetIOThread()
{
	this->Stop();
}

void NetIOThread::StartUp()
{
	if (nullptr == _work_thread)
	{
		_IsStart = true;
		_work_thread = new std::thread(&NetIOThread::Run, this);		
	}
}

void NetIOThread::Stop()
{
	if (_work_thread)
	{
		_work_thread->join();
		_work_thread = nullptr;
		_IsStart = false;
	}
}

bool NetIOThread::UpdataClientList()
{
	while (_client_num > 0 && !_offline_client_queue.empty())
	{
		std::lock_guard<std::mutex> lock(_lock);
		const auto offline_client = _offline_client_queue.front();
		if (offline_client)
		{
			_selctor.UnRegisterEvent(offline_client, SelectionKey<TcpStream>::OP_MAX);
			TcpNetMgr::Instance().OnDisConnect(_io_thread_id, offline_client->GetID());
			--_client_num;
		}
		_offline_client_queue.pop();
	}

	std::unique_lock<std::mutex> uni_lock(_lock);
	while (_client_num <= 0 && _new_client_queue.empty())
		_lock_cond.wait(uni_lock);
	if (uni_lock.owns_lock()) uni_lock.unlock();

	while (!_new_client_queue.empty())
	{
		std::lock_guard<std::mutex> lock(_lock);
		const auto new_client = _new_client_queue.front();
		_selctor.RegisterEvent(new_client, SelectionKey<TcpStream>::OP_READ);
		_selctor.RegisterEvent(new_client, SelectionKey<TcpStream>::OP_WRITE);
		_new_client_queue.pop();
		++_client_num;
	}

	return true;
}

void NetIOThread::Run()
{
	while (_IsStart)
	{
		if (UpdataClientList())
		{
			if (_selctor.Select(0) < 0)
			{
				return;
			}

			auto selected_keys = _selctor.GetSelectedKeys();
			if (nullptr != selected_keys && !selected_keys->empty())
			{
				for (auto key : *selected_keys)
				{
					bool is_close_tunnel = false;
					auto tunnel = key.GetTunnel();
					if (nullptr == tunnel) continue;
					
					if (key.IsReadable() && 
						!(TcpNetMgr::Instance().OnRecvData(_io_thread_id, tunnel->GetID())))
					{						
						is_close_tunnel = true;
					}
					
					if (_timer.GetElapsedMilliSecond() > 50.0)
					{
						if (key.IsWritable() && 
							!(TcpNetMgr::Instance().OnSendData(_io_thread_id, tunnel->GetID())))
						{
							is_close_tunnel = true;							
						}
						_timer.Updata();
					}

					if (is_close_tunnel)
					{
						this->RemoveClient(tunnel);						
					}
				}

				selected_keys->clear();
			}
		}
	}
}

void NetIOThread::AddClient(TcpStream* client)
{
	std::lock_guard<std::mutex> lock(_lock);
	_new_client_queue.push(client);
	_lock_cond.notify_one();
}

void NetIOThread::RemoveClient(TcpStream* client)
{
	std::lock_guard<std::mutex> lock(_lock);
	_offline_client_queue.push(client);
	_lock_cond.notify_one();
}

size_t NetIOThread::GetClientNum()
{
	std::lock_guard<std::mutex> lock(_lock);
	return _client_num;
}
