#include "TcpNetMgr.h"
#include "TcpNetTask.h"
#include "Message.hpp"
#include "Buffer.hpp"
#include <stdio.h>

TcpNetMgr::TcpNetMgr() 
	: _acceptor(nullptr), _used_id(-1), _callback(nullptr), _recv_byte_num(0), _send_byte_num(0), _client_num(0),
	  _IsStart(false), _this_thread(nullptr)
{
	_client_list.fill(nullptr);
}

TcpNetMgr& TcpNetMgr::Instance()
{
	static TcpNetMgr inst;
	return inst;
}

TcpNetMgr::~TcpNetMgr()
{
	this->Stop();
}

void TcpNetMgr::Stop()
{
	std::lock_guard<std::mutex> lock(_lock);
	for (int i = 0; i < _client_num; ++i)
	{
		_client_list[i]->Close();
		delete _client_list[i];
	}	

	if (_acceptor)
	{
		_acceptor->Stop();
		delete _acceptor;
		_acceptor = nullptr;
	}

	_IsStart = false;
	if(_this_thread)_this_thread->join();
}

bool TcpNetMgr::Init(unsigned short port, const char* ip)
{
	_acceptor = new TcpAcceptor(port, ip);

	if (nullptr == _acceptor || _acceptor->StartUp() < 0)
	{
		return false;
	}

	return true;
}

void TcpNetMgr::StartUp(size_t io_thread_num, int task_thread_num)
{
	if(nullptr == _this_thread)
	{
		_IsStart = true;
		_this_thread = new std::thread(&TcpNetMgr::Run, this, io_thread_num, task_thread_num);		
	}
}

bool TcpNetMgr::Run(size_t io_thread_num, int task_thread_num)
{
	if (nullptr == _acceptor  || !_acceptor->IsRun())
	{
		if (!Init(8035))
		{
			return false;
		}
	}

	_selector.RegisterEvent(_acceptor, SelectionKey<TcpAcceptor>::OP_ACCEPT);
	for (int i = 0; i < (int)io_thread_num; ++i)
	{
		const auto t = new NetIOThread(i);
		_work_thread_list.push_back(t);
	}

	for (int i = 0; i < (int)task_thread_num; ++i)
	{
		const auto t = new NetMsgThread(i);
		_net_msg_thread_list.push_back(t);
	}

	return this->Loop();
}

bool TcpNetMgr::Loop()
{
	while (_IsStart && _acceptor && _acceptor->IsRun())
	{
		this->TotalMessageSize();
		if (_selector.Select(0) < 0) {
			Log::Printf(Log::ErrorLevel, "select() is error.\n");
			_acceptor->Stop();
			return false;
		}

		auto selected_keys = _selector.GetSelectedKeys();
		if (nullptr != selected_keys)
		{
			for (auto key : *selected_keys)
			{
				if (key.IsAcceptable())
				{
					auto tunnel = key.GetTunnel();
					TcpStream* stream = tunnel->Accept();
					if (stream)
					{
						this->AddClient(stream);
						this->AddClientForWorkThread(stream);
						Log::Printf(Log::DebugLevel, "current conneted client num:%d.\n", (int)_client_num);
					}
				}
			}

			selected_keys->clear();
		}
	}

	return true;
}

void TcpNetMgr::AddClientForWorkThread(TcpStream* stream)
{
	int min_client_num = _work_thread_list[0]->GetClientNum();
	int thread_index = 0;
	for (int i = 1; i < (int)_work_thread_list.size(); ++i)
	{
		if (min_client_num > (int)_work_thread_list[i]->GetClientNum())
		{
			min_client_num = _work_thread_list[i]->GetClientNum();
			thread_index = i;
		}
	}

	_work_thread_list[thread_index]->AddClient(stream);
}

void TcpNetMgr::RemoveClientForWorkThread(int io_thread_id, NetID id)
{
	if (io_thread_id < 0 || io_thread_id >= (int)_work_thread_list.size()
		|| id < 0 || id >= (int)_client_list.size())
	{
		return;
	}

	_work_thread_list[io_thread_id]->RemoveClient(_client_list[id]);
}

void TcpNetMgr::AddClient(TcpStream* stream)
{
	std::lock_guard<std::mutex> lock(_lock);
	int id = -1;
	if (!_free_netid.empty()) {
		id = _free_netid.top();
		_free_netid.pop();
	}else if (_used_id < 65536) {
		id = ++_used_id;
	}

	if (id == -1) {		
		return;
	}

	stream->SetID(id);
	_client_list[id] = stream;
	++_client_num;
	this->OnAccept(id);
}

void TcpNetMgr::RemoveClient(NetID id)
{
	std::lock_guard<std::mutex> lock(_lock);
	if (id >= 0 && id < (int)_client_list.size() && _client_list[id])
	{
		Log::Printf(Log::DebugLevel, "client is logouting.... current client num: %d.\n", (int)_client_num);
		_client_list[id]->Close();
		delete _client_list[id];		
		_client_list[id] = nullptr;	
		_free_netid.push(id);		
		--_client_num;
		Log::Printf(Log::DebugLevel, "client is logout successful. current client num: %d.\n", (int)_client_num);
	}
}

TcpStream* TcpNetMgr::GetTunnel(NetID id)
{
	if (id < 0 || id >= (int)_client_list.size()) return nullptr;		

	return _client_list[id];
}

Buffer* TcpNetMgr::GetTunnelRecvBuffer(NetID id)
{
	std::lock_guard<std::mutex> lock(_lock);
	if (id < 0 || id >= (int)_client_list.size()) return nullptr;
	if (_client_list[id])
	{
		return _client_list[id]->GetRecvBuff();
	}

	return nullptr;
}

void TcpNetMgr::TotalMessageSize()
{
	double elapsed_second = _timer.GetElapsedSecond();
	if (elapsed_second >= 1.0)
	{
		elapsed_second = _timer.GetElapsedSecond();
		int reveive_speed = (int)(_recv_byte_num * 1.0 / elapsed_second);
		int send_speed  = (int)(_send_byte_num * 1.0 / elapsed_second);
		int band_width = (int)((((_recv_byte_num + send_speed) * 1.0) / (elapsed_second * 1024 * 1024)) * 8.0);
		Log::Printf(Log::DebugLevel, "receive client message. <reveive-speed: %d byte/s, send-speed: %d byte/s, band-width: %d Mbps>.\n",
			reveive_speed, send_speed, band_width);

		_timer.Updata();
		_recv_byte_num = 0;
		_send_byte_num = 0;
	}
}

bool TcpNetMgr::SendNetMsg(NetID id, Protocal::MessageHeader *header)
{	
	std::lock_guard<std::mutex> lock(_buffer_lock);
	if (id < 0 || nullptr == header)  return false;

	auto tunnel = TcpNetMgr::Instance().GetTunnel(id);
	if (nullptr == tunnel)return false;

	auto send_buffer = tunnel->GetSendBuff();
	if (nullptr == send_buffer) return false;

	_send_byte_num += header->msg_size;
	return send_buffer->Push((const char*)header, header->msg_size);
}

void TcpNetMgr::OnAccept(NetID id)
{
	if (_callback) _callback->OnAccept(id);
}

bool TcpNetMgr::OnNetMsg(int io_thread_id, NetID id)
{
	std::lock_guard<std::mutex> lock(_lock);	
	if (id < 0 || id >= (int)_offline_client_list.size() ||
		id >= (int)_client_list.size() || _offline_client_list[id])
	{
		return false;
	}

	Buffer* recv_buffer = (nullptr != _client_list[id]) ? _client_list[id]->GetRecvBuff() : nullptr;
	if (nullptr == recv_buffer || recv_buffer->Size() <= 0)
	{
		return false;
	}

	while (recv_buffer->Size() >= sizeof(Protocal::MessageHeader))
	{
		Protocal::MessageHeader *header = reinterpret_cast<Protocal::MessageHeader*>(recv_buffer->Begin());
		if (recv_buffer->Size() >= header->msg_size) {
			int old_msg_size = header->msg_size;
			if (_callback) _callback->OnNetMsg(id, header);
			if (-1 == recv_buffer->RemoveTop(old_msg_size))
			{
				Log::Printf(Log::ErrorLevel, "Buffer::RemoveTop() failed.\n");
				return false;
			}

			_recv_byte_num += header->msg_size;
		}
		else {
			break;
		}
	}

	_lock_cond.notify_one();
	return true;
}

bool TcpNetMgr::OnRecvData(int io_thread_id, NetID id)
{
	std::unique_lock<std::mutex> lock(_lock);
	if (id < 0 && id >= (int)_client_list.size())
	{		
		return false;
	}	

	TcpStream* stream = _client_list[id];	
	Buffer* recv_buffer = stream->GetRecvBuff();
	if (nullptr == recv_buffer || stream->ReadData() < 0)
	{
		return false;
	}

	if (!_net_msg_thread_list[std::rand() % _net_msg_thread_list.size()]->Push(new NetMsgTask(io_thread_id, id)))
	{
		return false;
	}

	_lock_cond.wait(lock);
	return true;
}

bool TcpNetMgr::OnSendData(int io_thread_id, NetID id)
{
	std::lock_guard<std::mutex> lock(_lock);
	std::lock_guard<std::mutex> lock2(_buffer_lock);
	if (id < 0 && id >= (int)_client_list.size())
	{
		return false;
	}
	
	int len = _client_list[id]->WriteData();
	if (len < 0)
	{
		return false;
	}
	else if(len > 0)
	{
		//if (_callback) _callback->OnSendData(id);
	}	
	
	return true;
}

void TcpNetMgr::OnDisConnect(int io_thread_id, NetID id)
{
	this->RemoveClient(id);
	if (_callback) _callback->OnDisConnect(id);
}

