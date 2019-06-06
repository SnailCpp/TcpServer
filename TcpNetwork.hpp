#ifndef _TCP_STREAM_HPP
#define _TCP_STREAM_HPP

#ifdef _WIN32
	#define WIN32_LEAN_AND_MEAN
	#define _WINSOCK_DEPRECATED_NO_WARNINGS
	#define  _CRT_SECURE_NO_WARNINGS
	#define FD_SETSIZE 1024 * 10
	#pragma comment(lib, "ws2_32.lib")
	typedef int socklen_t;

	#include <WinSock2.h>
#else
	#define SOCKET unsigned int
	#define INVALID_SOCKET (unsigned int)~0
	#define SOCKET_ERROR (-1)
	typedef unsigned int socklen_t;

	#include <unistd.h>
	#include <arpa/inet.h>
#endif

#include "Log.hpp"
#include "Buffer.hpp"
#include "Message.hpp"
#include <stdio.h>
#include <sstream>
#include <string>
#include <mutex>

typedef int NetID;

class TcpStream;

class ITcpNetCallback
{
public:
	ITcpNetCallback() = default;
	virtual ~ITcpNetCallback() = default;

	virtual void OnAccept(NetID id) = 0;
	virtual void OnNetMsg(NetID id, Protocal::MessageHeader* header) = 0;
	virtual void OnDisConnect(NetID id) = 0;
};

template<class T> class TcpNetCallbackImpl : public ITcpNetCallback
{
public:
	TcpNetCallbackImpl(T* cllback = nullptr) : _callback(cllback) {}
	virtual ~TcpNetCallbackImpl() override = default;

	virtual void OnAccept(NetID id) override
	{
		if (_callback)_callback->OnAccept(id);
	}

	virtual void OnNetMsg(NetID id, Protocal::MessageHeader* header) override
	{
		if (_callback)_callback->OnNetMsg(id, header);
	}	

	virtual void OnDisConnect(NetID id) override
	{
		if (_callback)_callback->OnDisConnect(id);
	}

private:
	T* _callback;
};

class TcpNetInterface
{
public:
	TcpNetInterface(unsigned short port, const char* ip = nullptr) 
		: _sock(INVALID_SOCKET), _port(port), _ip(ip), _peer_port(-1)
	{
		memset(_peer_ip, 0, sizeof(_peer_ip));
	}

	virtual ~TcpNetInterface() = default;

	virtual SOCKET InitSocket() = 0;
	virtual bool Bind() = 0;
	virtual bool Listen(int n = SOMAXCONN) = 0;
	virtual SOCKET Accept() = 0;
	virtual bool Connect() = 0;
	virtual void Close() = 0;

protected:
	SOCKET					_sock;
	unsigned short 			_port;
	const char*				_ip;
	unsigned short 			_peer_port;
	char					_peer_ip[32];
};

class TcpNetHander : public TcpNetInterface
{
	friend class TcpAcceptor;
	friend class TcpConnector;
public:
	TcpNetHander(unsigned short port, const char* ip = nullptr) :TcpNetInterface(port, ip) {	}
	~TcpNetHander() override = default;

	virtual SOCKET InitSocket() override
	{
#ifdef _WIN32
		WORD version = MAKEWORD(2, 2);
		WSADATA wsadata;
		WSAStartup(version, &wsadata);
#endif
		if (INVALID_SOCKET != _sock) this->Close();

		_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		return _sock;
	}

	virtual bool Bind() override
	{
		sockaddr_in server_addr = {};

		if (_ip) {
			server_addr.sin_addr.s_addr = inet_addr(_ip);
		}else {
			server_addr.sin_addr.s_addr = INADDR_ANY;
		}

		server_addr.sin_port = htons(_port);
		server_addr.sin_family = AF_INET;
		int ret = bind(_sock, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr));
		return  SOCKET_ERROR != ret;
	}

	virtual bool Listen(int n = SOMAXCONN) override
	{
		int ret = listen(_sock, n);
		return  SOCKET_ERROR != ret;
	}

	virtual SOCKET Accept() override
	{
		SOCKET client_socket = INVALID_SOCKET;
		sockaddr_in client_addr = {};
		socklen_t addr_len = sizeof(sockaddr_in);
		client_socket = accept(_sock, reinterpret_cast<sockaddr*>(&client_addr), &addr_len);
		strcpy(_peer_ip, inet_ntoa(client_addr.sin_addr));
		_peer_port = ntohs(client_addr.sin_port);
		return client_socket;
	}

	virtual bool Connect() override
	{
		if (INVALID_SOCKET == _sock) this->InitSocket();
		
		sockaddr_in server_addr = {};
		if (_ip) {
			server_addr.sin_addr.s_addr = inet_addr(_ip);
		}else {
			server_addr.sin_addr.s_addr = INADDR_ANY;
		}
		server_addr.sin_port = htons(_port);
		server_addr.sin_family = AF_INET;

		int ret = connect(_sock, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr));
		return SOCKET_ERROR != ret;
	}

	virtual void Close() override
	{
		if (INVALID_SOCKET != _sock) 
		{
#ifdef _WIN32
			closesocket(_sock);
			WSACleanup();
#else
			close(_sock);
#endif	
			_sock = INVALID_SOCKET;
		}
	}
};

class TcpStream
{
public:
	TcpStream(SOCKET sock = INVALID_SOCKET, size_t size = 0)
		: _id(-1), _socket(sock), _recv_buffer(size), _send_buffer(size) {}

	~TcpStream() = default;

	friend TcpStream& operator >> (TcpStream& stream, Buffer& recv_buffer)
	{
		if (!recv_buffer.IsFull())
		{
			const int old_size = recv_buffer.Size();
			const int free_size = recv_buffer.GetFreeSize();
			char* begin = recv_buffer.Begin();
			if (nullptr == begin || free_size <= 0)
			{
				Log::Printf(Log::ErrorLevel, "No enough read buffer space on <socket:%d>.\n", (int)stream._socket);
				throw -1;
			}

			int bytes = recv(stream._socket, begin + old_size, free_size, 0);
			if (bytes <= 0) {
				throw -2;
			}

			if (-1 == recv_buffer.Resize(bytes + old_size))
			{
				Log::Printf(Log::ErrorLevel, "Buffer::Resize() failed.\n");
				throw -3;
			}
		}

		return stream;
	}

	int ReadData()
	{	
		if (!_recv_buffer.IsFull())
		{
			const int old_size = _recv_buffer.Size();
			const int free_size = _recv_buffer.GetFreeSize();
			char* begin = _recv_buffer.Begin();
			if (nullptr == begin || free_size <= 0)
			{
				Log::Printf(Log::ErrorLevel, "No enough read buffer space on <socket:%d>.\n", (int)_socket);
				return -1;
			}

			int bytes = recv(_socket, begin + old_size, free_size, 0);
			if (bytes <= 0) {
				return -2;
			}			
			
			if (-1 == _recv_buffer.Resize(bytes + old_size))
			{
				Log::Printf(Log::ErrorLevel, "Buffer::Resize() failed.\n");
				return -3;
			}
		}		
		
		return 0;
	}

	int WriteData()
	{
		if (_send_buffer.IsEmpty()) return 0;

		int send_len = _send_buffer.Size() < 10240? _send_buffer.Size() : 10240;
		int bytes = send(_socket, _send_buffer.Begin(), send_len, 0);
		if (bytes > 0)
		{
			_send_buffer.RemoveTop(bytes);
			return bytes;
		}

		return -1;
	}

	void Close()
	{
		if (INVALID_SOCKET != _socket) {			
#ifdef _WIN32
			closesocket(_socket);
#else
			close(_socket);
#endif
			_socket = INVALID_SOCKET;			
		}
	}	

	void SetID(NetID id)
	{
		_id = id;
	}

	NetID GetID()
	{
		return _id;
	}	

	SOCKET GetSocket()
	{
		return _socket;
	}

	Buffer* GetRecvBuff()
	{
		return (&_recv_buffer);
	}

	Buffer* GetSendBuff()
	{		
		return (&_send_buffer);
	}	

private:
	NetID				_id;
	SOCKET				_socket;
	Buffer				_recv_buffer;
	Buffer				_send_buffer;
};

class TcpAcceptor
{
public:
	TcpAcceptor(unsigned short port, const char* ip = nullptr)
		:_net_hander(port, ip), _sock(INVALID_SOCKET) {}	

	~TcpAcceptor()
	{
		this->Stop();
	}

	bool IsRun()
	{
		return _sock != INVALID_SOCKET;
	}
	
	int StartUp()
	{
		_sock = _net_hander.InitSocket();
		if (INVALID_SOCKET != _sock) {
			Log::Printf(Log::InfoLevel, "socket() is successful. <local-socket:%d>.\n", _net_hander._sock);
		}else {
			Log::Printf(Log::ErrorLevel, "socket() is failed.\n");
			_net_hander.Close();
			return -1;
		}

		if (_net_hander.Bind()) {
			if (nullptr == _net_hander._ip){
				Log::Printf(Log::InfoLevel, "bind() is successful. <local-ip:127.0.0.1, local-port:%d>.\n", _net_hander._port);
			}else {
				printf("[INFO] bind() is successful. <local-ip:%s, local-port:%d>.\n", _net_hander._ip, _net_hander._port);
			}			
		}else {
			Log::Printf(Log::ErrorLevel, "bind() is failed. <local-port:%d>.\n", _net_hander._port);
			_net_hander.Close();
			return -2;
		}

		if (_net_hander.Listen()) {
			Log::Printf(Log::InfoLevel, "listen() is successful.\n");
		}else {
			Log::Printf(Log::ErrorLevel, "listen() is failed.\n");
			_net_hander.Close();
			return -3;
		}

		return 0;
	}

	void Stop()
	{
		_net_hander.Close();
		_sock = INVALID_SOCKET;
	}

	TcpStream* Accept()
	{
		if (IsRun())
		{
			SOCKET client_socket = _net_hander.Accept();
			if (INVALID_SOCKET == client_socket)
			{
				Log::Printf(Log::ErrorLevel, "accept() is failed.\n");
				return nullptr;
			}else {
				Log::Printf(Log::InfoLevel, "accept() is successful. <client-scoket:%d, client-ip:%s, client-port:%d>.\n",
					(int)client_socket, _net_hander._peer_ip, _net_hander._peer_port);
			}

			return new TcpStream(client_socket, 10240);
		}

		return nullptr;
	}

	SOCKET GetSocket()
	{
		return _net_hander._sock;
	}
	
private:
	SOCKET				_sock;
	TcpNetHander		_net_hander;
};

class TcpConnector
{
public:
	TcpConnector(unsigned short port, const char* ip = nullptr)
		:_net_hander(port, ip), _sock(INVALID_SOCKET) {}

	~TcpConnector()
	{
		this->Stop();
	}

	bool IsRun()
	{
		return _sock != INVALID_SOCKET;
	}

	int StartUp()
	{
		_sock = _net_hander.InitSocket();
		if (INVALID_SOCKET != _sock) {
			Log::Printf(Log::InfoLevel, "socket() is successful. <local-socket:%d>.\n", _sock);
		}else {
			Log::Printf(Log::ErrorLevel, "socket() is failed.\n");
			_net_hander.Close();
			return -1;
		}		

		return 0;
	}

	TcpStream* Connect()
	{
		if (_net_hander.Connect())
		{
			Log::Printf(Log::InfoLevel, "TcpClient::Connect() is successful.  <server-ip:%s, server-port:%d>.\n", _net_hander._ip, _net_hander._port);
		}else {
			Log::Printf(Log::ErrorLevel, "TcpClient::Connect() is failed.\n");
			_net_hander.Close();
			return nullptr;
		}

		return new TcpStream(_sock, 2048);
	}

	void Stop()
	{
		_net_hander.Close();
		_sock = INVALID_SOCKET;
	}	

private:
	SOCKET				_sock;
	TcpNetHander		_net_hander;
};

#endif