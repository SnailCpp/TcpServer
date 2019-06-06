#ifndef _MESSAGE_HPP_
#define _MESSAGE_HPP_

#pragma pack (4)  
namespace Protocal
{
	enum MESSAGE_TYPE
	{
		MT_INVALID = 0,
		MT_LOGIN_REQUEST,
		MT_LOGIN_RESULT,
	};

	class MessageHeader
	{
	public:
		MessageHeader(unsigned int size = 0, unsigned int type = MT_INVALID) : msg_size(size), msg_type(type) {}
		~MessageHeader() {}

		unsigned int msg_size;
		unsigned int msg_type;
	};

	class CSLoginRequest : public MessageHeader
	{
	public:
		CSLoginRequest() : MessageHeader(sizeof(CSLoginRequest), MT_LOGIN_REQUEST) {}
		~CSLoginRequest() {}

		char username[32];
		char password[32];
		char data[1024];
	};

	class SCLoginResult : public MessageHeader
	{
	public:
		SCLoginResult() : MessageHeader(sizeof(SCLoginResult), MT_LOGIN_RESULT) {}
		~SCLoginResult() {}

		int result;
		char data[1024];
	};
};
#pragma pack ()  

#endif