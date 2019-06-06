#include "TcpServer.h"
#include <stdio.h>

constexpr unsigned short PORT = 8035;

int main()
{
	printf("it is windows server? 0.false 1.true\n");
	int is_win_server = 1;
	scanf("%d", &is_win_server);
	
	char *ip = new char[32];
	if (is_win_server) {
		ip = nullptr;
	}else{
		printf("please enter server ip:\n");
		scanf("%s", ip);
	}	

	TcpServer server;
	server.Init(PORT, ip);
	server.Start();
	Log::Printf(Log::InfoLevel, "server is shutdown.\n");

	if (ip)
		delete[] ip;

	return 0;
}