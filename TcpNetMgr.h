#ifndef _TCP_NET_MGR_H_
#define _TCP_NET_MGR_H_

#include "TcpNetwork.hpp"
#include "Selector.hpp"
#include "Timer.hpp"
#include <atomic>
#include <mutex>
#include <vector>
#include <list>
#include <array>
#include <set>
#include <queue>

class NetIOThread;
class NetMsgThread;
class ITcpNetCallback;

class TcpNetMgr
{
	TcpNetMgr();

public:
	static TcpNetMgr& Instance();
	virtual ~TcpNetMgr();

	void Stop();
	bool Init(unsigned short port, const char* ip = nullptr);
	void StartUp(size_t io_thread_num = 1, int task_thread_num = 1);
	bool Run(size_t io_thread_num, int task_thread_num);
	bool Loop();

	void AddClientForWorkThread(TcpStream* stream);
	void RemoveClientForWorkThread(int io_thread_id, NetID id);
	void AddClient(TcpStream* stream);
	void RemoveClient(NetID id);
	TcpStream* GetTunnel(NetID id);
	Buffer* GetTunnelRecvBuffer(NetID id);
	void TotalMessageSize();

	bool SendNetMsg(NetID id, Protocal::MessageHeader *header);

	void OnAccept(NetID id);
	bool OnNetMsg(int io_thread_id, NetID id);
	bool OnRecvData(int io_thread_id, NetID id);
	bool OnSendData(int io_thread_id, NetID id);
	void OnDisConnect(int io_thread_id, NetID id);

	// ×¢²áÍøÂç»Øµ÷ 
	void RegisterNetCallback(ITcpNetCallback* callback)
	{
		_callback = callback;
	}

private:
	bool								_IsStart;
	std::thread*						_this_thread;
	ITcpNetCallback*					_callback;
	std::vector<NetIOThread*>			_work_thread_list;
	std::vector<NetMsgThread*>			_net_msg_thread_list;
	Selector<TcpAcceptor>				_selector;
	std::array<TcpStream*, 65535>		_client_list;
	std::array<TcpStream*, 65535>		_offline_client_list;
	std::atomic<int>					_client_num;
	std::atomic<int>					_recv_byte_num;
	std::atomic<int>					_send_byte_num;
	TcpAcceptor*						_acceptor;
	std::atomic<int>					_used_id;
	std::mutex							_lock;
	std::mutex							_buffer_lock;
	std::condition_variable				_lock_cond;
	Timer								_timer;
	std::priority_queue
	<NetID, std::vector<int>, std::greater<NetID> >	_free_netid;
};

class EngineAdapter
{
public:
	static bool SendNetMsg(NetID id, Protocal::MessageHeader *header)
	{
		return TcpNetMgr::Instance().SendNetMsg(id, header);
	}

	static void DisConnect(NetID id)
	{
		if (id < 0)  return;
		TcpNetMgr::Instance().OnDisConnect(-1, id);
	}
};
#endif
