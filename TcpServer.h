#ifndef _TCP_SERVER_H_
#define _TCP_SERVER_H_

#include "TcpNetwork.hpp"

class TcpServer
{
public:
	TcpServer();
	~TcpServer();

	void Init(unsigned short port, const char* ip = nullptr);
	void Loop();
	void Start();
	void Stop();

	void OnAccept(NetID id);
	void OnNetMsg(NetID id, Protocal::MessageHeader *header);
	void OnDisConnect(NetID id);

private:
	TcpNetCallbackImpl<TcpServer> _callback_impl;	
};

#endif