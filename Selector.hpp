#ifndef	_SELECTOR_HPP
#define _SELECTOR_HPP

#include "TcpNetwork.hpp"
#include <vector>
#include <map>

template <class T> class Selector;

template <class T>
class SelectionKey
{
	friend class Selector <T>;
public:
	enum Event
	{
		OP_INVALID = 0x0000,
		OP_CONNECT = 0x0001,
		OP_ACCEPT = 0x0010,
		OP_READ = 0x0100,
		OP_WRITE = 0x1000,
		OP_MAX = 0x1111
	};

	SelectionKey() :_tunnel(nullptr)
	{
		FD_ZERO(&_readfds);
		FD_ZERO(&_writefds);
		FD_ZERO(&_exceptfds);
	}

	~SelectionKey() = default;

	bool IsConnectable()
	{
		return this->IsWritable();
	}

	bool IsAcceptable()
	{
		return this->IsReadable();
	}

	bool IsReadable()
	{
		if (nullptr == _tunnel)
		{
			return false;
		}

		return (0 != FD_ISSET(_tunnel->GetSocket(), &_readfds));
	}

	bool IsWritable()
	{
		if (nullptr == _tunnel)
		{
			return false;
		}

		return (0 != FD_ISSET(_tunnel->GetSocket(), &_writefds));
	}

	bool IsException()
	{
		if (nullptr == _tunnel)
		{
			return false;
		}

		return (0 != FD_ISSET(_tunnel->GetSocket(), &_exceptfds));
	}

	T* GetTunnel()
	{
		return _tunnel;
	}

private:
	fd_set _readfds;
	fd_set _writefds;
	fd_set _exceptfds;
	T* _tunnel;
};

template <class T>
class Selector
{
public:
	Selector()
	{
		FD_ZERO(&_readfds);
		FD_ZERO(&_writefds);
		FD_ZERO(&_exceptfds);
	}

	~Selector() = default;

	int Select(long timeout)
	{
		SelectionKey<T> key;
		memcpy(&key._readfds, &_readfds, sizeof(_readfds));
		memcpy(&key._writefds, &_writefds, sizeof(_writefds));
		memcpy(&key._exceptfds, &_exceptfds, sizeof(_exceptfds));

		timeval tv = {};
		tv.tv_sec = timeout;
		tv.tv_usec = 0;

		int MAX_FD_SIZE = _tunnel_list.empty() ? FD_SETSIZE : (*(_tunnel_list.crbegin())).first + 1;
		int ret = select(MAX_FD_SIZE, &key._readfds, &key._writefds, &key._exceptfds, &tv);
		if (ret > 0)
		{
			for (auto tunnel : _tunnel_list)
			{
				key._tunnel = tunnel.second;
				if (key.IsReadable() || key.IsWritable() || key.IsException())
				{
					_selection_keys.push_back(key);
				}
			}
		}

		return ret;
	}


	int Select()
	{
		SelectionKey<T> key;
		memcpy(&key._readfds, &_readfds, sizeof(_readfds));
		memcpy(&key._writefds, &_writefds, sizeof(_writefds));
		memcpy(&key._exceptfds, &_exceptfds, sizeof(_exceptfds));

		int MAX_FD_SIZE = _tunnel_list.empty() ? FD_SETSIZE : (*(_tunnel_list.crbegin())).first + 1;
		int ret = select(MAX_FD_SIZE, &key._readfds, &key._writefds, &key._exceptfds, nullptr);
		if (ret > 0)
		{
			for (auto tunnel : _tunnel_list)
			{
				key._tunnel = tunnel.second;
				if (key.IsReadable() || key.IsWritable() || key.IsException())
				{
					_selection_keys.push_back(key);
				}
			}
		}

		return ret;
	}

	std::vector<SelectionKey<T> >* GetSelectedKeys()
	{
		return &_selection_keys;
	}

	void RegisterEvent(T* tunnel, enum SelectionKey<T>::Event event)
	{
		if (nullptr == tunnel || event <= SelectionKey<T>::OP_INVALID || event > SelectionKey<T>::OP_MAX)
		{
			return;
		}

		int fd = tunnel->GetSocket();
		if (event == SelectionKey<T>::OP_ACCEPT) {
			FD_SET(fd, &_readfds);
			FD_SET(fd, &_exceptfds);
		}
		else if (event == SelectionKey<T>::OP_READ) {
			FD_SET(fd, &_readfds);
		}
		else if (event == SelectionKey<T>::OP_WRITE || event == SelectionKey<T>::OP_CONNECT) {
			FD_SET(fd, &_writefds);
		}
		else if (event ==
			(SelectionKey<T>::OP_READ | SelectionKey<T>::OP_WRITE)) {
			FD_SET(fd, &_readfds);
			FD_SET(fd, &_writefds);
		}
		else if (event == SelectionKey<T>::OP_MAX)
		{
			FD_SET(fd, &_exceptfds);
			FD_SET(fd, &_readfds);
			FD_SET(fd, &_writefds);
		}

		_tunnel_list[fd] = tunnel;
	}

	void UnRegisterEvent(T* tunnel, enum SelectionKey<T>::Event event)
	{
		if (nullptr == tunnel || event <= SelectionKey<T>::OP_INVALID || event > SelectionKey<T>::OP_MAX)
		{
			return;
		}

		int fd = tunnel->GetSocket();
		if (event == SelectionKey<T>::OP_ACCEPT) {
			FD_CLR(fd, &_readfds);
			FD_CLR(fd, &_exceptfds);
		}
		else if (event == SelectionKey<T>::OP_READ) {
			FD_CLR(fd, &_readfds);
		}
		else if (event == SelectionKey<T>::OP_WRITE || event == SelectionKey<T>::OP_CONNECT) {
			FD_CLR(fd, &_writefds);
		}
		else if (event ==
			(SelectionKey<T>::OP_READ | SelectionKey<T>::OP_WRITE)) {
			FD_CLR(fd, &_readfds);
			FD_CLR(fd, &_writefds);
		}
		else if (event == SelectionKey<T>::OP_MAX)
		{
			FD_CLR(fd, &_exceptfds);
			FD_CLR(fd, &_readfds);
			FD_CLR(fd, &_writefds);
		}

		if (FD_ISSET(fd, &_exceptfds) || FD_ISSET(fd, &_readfds) || FD_ISSET(fd, &_writefds))
		{
			return;
		}

		if (_tunnel_list.find(fd) != _tunnel_list.end())
		{
			_tunnel_list.erase(fd);
		}
	}

private:
	fd_set _readfds;
	fd_set _writefds;
	fd_set _exceptfds;
	std::vector<SelectionKey<T> > _selection_keys;
	std::map<SOCKET, T*>		  _tunnel_list;
};
#endif