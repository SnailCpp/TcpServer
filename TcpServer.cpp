#include "TcpServer.h"
#include "TcpNetMgr.h"
#include "log.hpp"

TcpServer::TcpServer()
	:_callback_impl(this)
{

}

TcpServer::~TcpServer()
{

}

void TcpServer::Init(unsigned short port, const char* ip)
{
	TcpNetMgr::Instance().Init(port, ip);
	TcpNetMgr::Instance().RegisterNetCallback(&_callback_impl);
}

void TcpServer::Loop()
{
	while (true)
	{
#ifdef _WIN32
		Sleep(3000);
#else
		sleep(3)
#endif		
	}	
}

void TcpServer::Start()
{
	TcpNetMgr::Instance().StartUp(1, 2);
	this->Loop();

}

void TcpServer::Stop()
{
	TcpNetMgr::Instance().Stop();
}

void TcpServer::OnAccept(NetID id)
{
	Log::Printf(Log::InfoLevel, "TcpServer::OnAccept(). <id: %d>.\n", id);
}

void TcpServer::OnNetMsg(NetID id, Protocal::MessageHeader *header)
{
	if (nullptr == header || id < 0)
	{
		return;
	}

	switch (header->msg_type)
	{
	case Protocal::MT_LOGIN_REQUEST:
	{
		Protocal::CSLoginRequest *login = reinterpret_cast<Protocal::CSLoginRequest*>(header);
		Log::Printf(Log::DebugLevel, "receive client message. <net-id:%d, message-type:MT_LOGIN_REQUEST, data-length:%d, username:%s, password:%s>\n",
			(int)id, header->msg_size, login->username, login->password);
		Protocal::SCLoginResult result = {};
		result.result = 1;
		//EngineAdapter::SendNetMsg(id, &result);
	}
	break;

	default:
		break;
	}
}

void TcpServer::OnDisConnect(NetID id)
{
	Log::Printf(Log::InfoLevel, "TcpServer::OnDisConnect(). <id: %d>.\n", id);
}

