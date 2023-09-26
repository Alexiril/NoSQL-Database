#include "ServerTCP.hpp"

using namespace SocketTCP;

DataBuffer TcpServer::Client::loadData()
{
	if (_status != SocketStatus::connected)
		return DataBuffer();
	using namespace std::chrono_literals;
	DataBuffer buffer;
	u64 size = 0;
	i64 err = 0;

	WIN(if (u_long t = true; SOCKET_ERROR == ioctlsocket(socket, FIONBIO, &t)) return DataBuffer();)
	i64 answ = recv(socket, (char *)&size, sizeof(size), NIX(MSG_DONTWAIT) WIN(0));
	WIN(if (u_long t = false; SOCKET_ERROR == ioctlsocket(socket, FIONBIO, &t)) return DataBuffer();)

	if (!answ)
	{
		disconnect();
		return DataBuffer();
	}
	else if (answ == -1)
	{
		WIN(
			err = convertError();
			if (!err) {
				SockLen_t len = sizeof(err);
				getsockopt(socket, SOL_SOCKET, SO_ERROR, WIN((char *)) & err, &len);
			})
		NIX(
			SockLen_t len = sizeof(err);
			getsockopt(socket, SOL_SOCKET, SO_ERROR, &err, &len);
			if (!err) err = errno;)
		switch (err)
		{
		case 0:
			break;
		case ETIMEDOUT:
		case ECONNRESET:
		case EPIPE:
			disconnect();
			[[fallthrough]];
		case EAGAIN:
			return DataBuffer();
		default:
			disconnect();
			std::cerr << "Unhandled error!\n"
					  << "Code: " << err NIX(<< " " << strerror(errno)) << '\n';
			return DataBuffer();
		}
	}

	if (!size)
		return DataBuffer();
	buffer.resize(size);
	recv(socket, (char *)buffer.data(), (i32)buffer.size(), 0);
	return buffer;
}

SocketStatus TcpServer::Client::disconnect()
{
	_status = SocketStatus::disconnected;
	if (socket == WIN(INVALID_SOCKET) NIX(-1))
		return _status;
	shutdown(socket, SD_BOTH);
	WIN(closesocket)
	NIX(close)
	(socket);
	socket = WIN(INVALID_SOCKET) NIX(-1);
	return _status;
}

bool TcpServer::Client::sendData(const char *buffer, const u64 size) const
{
	if (_status != SocketStatus::connected)
		return false;

	void *send_buffer = malloc(size + sizeof(u64));
	if (reinterpret_cast<char *>(send_buffer) + sizeof(u64) == 0)
		return false;

	memcpy(reinterpret_cast<char *>(send_buffer) + sizeof(u64), buffer, size);
	if (not send_buffer)
		return false;

	*reinterpret_cast<u64 *>(send_buffer) = size;

	if (send(socket, reinterpret_cast<char *>(send_buffer), (i32)(size + sizeof(u64)), 0) < 0)
		return false;

	free(send_buffer);
	return true;
}

TcpServer::Client::Client(Socket socket, SocketAddr_in _address) : _address(_address), socket(socket) {}

TcpServer::Client::~Client()
{
	if (socket == WIN(INVALID_SOCKET) NIX(-1))
		return;
	shutdown(socket, SD_BOTH);
	WIN(closesocket(socket);)
	NIX(close(socket);)
}

u32 TcpServer::Client::getHost() const { return NIX(_address.sin_addr.s_addr) WIN(_address.sin_addr.S_un.S_addr); }
u16 TcpServer::Client::getPort() const { return _address.sin_port; }

TcpServer::TcpServer(const u32 ipAddress, const u16 port,
					 KeepAliveConfig ka_conf,
					 handlerFunctionType handler,
					 connectionHandlerFunctionType connect_hndl,
					 connectionHandlerFunctionType disconnect_hndl,
					 u64 thread_count)
	: ipAddress(ipAddress),
	  port(port),
	  handler(handler),
	  connect_hndl(connect_hndl),
	  disconnect_hndl(disconnect_hndl),
	  _threads(thread_count),
	  ka_conf(ka_conf),
	  serv_socket(0)
{
}

TcpServer::~TcpServer()
{
	if (_status == status::up)
		stop();
}

void TcpServer::setHandler(TcpServer::handlerFunctionType handler)
{
	this->handler = handler;
}

u16 TcpServer::getPort() const { return port; }
u16 TcpServer::setPort(const u16 port)
{
	this->port = port;
	start();
	return port;
}

u32 SocketTCP::TcpServer::getIpAddress() const { return ipAddress; }
u32 SocketTCP::TcpServer::setIpAddress(const u32 ipAddress)
{
	this->ipAddress = ipAddress;
	start();
	return ipAddress;
}

TcpServer::status TcpServer::start()
{
	i32 flag;
	if (_status == status::up)
		stop();

	SocketAddr_in _address{};
	_address.sin_addr.s_addr = ipAddress;
	_address.sin_port = htons(port);
	_address.sin_family = AF_INET;

	if ((serv_socket = socket(AF_INET, SOCK_STREAM NIX(| SOCK_NONBLOCK), 0)) WIN(== INVALID_SOCKET) NIX(== -1))
		return _status = status::err_socket_init;

	WIN(
		if (u_long mode = 0; ioctlsocket(serv_socket, FIONBIO, &mode) == SOCKET_ERROR) {
			return _status = status::err_socket_init;
		})

	if (flag = true;
		(setsockopt(serv_socket, SOL_SOCKET, SO_REUSEADDR, WIN((char *)) & flag, sizeof(flag)) == -1) ||
		(bind(serv_socket, (struct sockaddr *)&_address, sizeof(_address)) WIN(== SOCKET_ERROR) NIX(< 0)))
		return _status = status::err_socket_bind;

	if (listen(serv_socket, SOMAXCONN) WIN(== SOCKET_ERROR) NIX(< 0))
		return _status = status::err_socket_listening;
	_status = status::up;
	_threads.addJob([this]
					{ handlingAcceptLoop(); });
	_threads.addJob([this]
					{ waitingDataLoop(); });
	return _status;
}

void TcpServer::stop()
{
	_threads.dropUnstartedJobs();
	_status = status::close;
	WIN(closesocket)
	NIX(close)
	(serv_socket);
	clientMutex.lock();
	clients.clear();
	clientMutex.unlock();
}

void TcpServer::joinLoop()
{
	_threads.join();
}

void SocketTCP::TcpServer::halt()
{
	halts = true;
}

bool TcpServer::connectTo(u32 host, u16 port, connectionHandlerFunctionType connect_hndl)
{
	Socket clientSocket;
	SocketAddr_in _address{};
	if ((clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_IP)) WIN(== INVALID_SOCKET) NIX(< 0))
		return false;

	new (&_address) SocketAddr_in;
	_address.sin_family = AF_INET;
	_address.sin_addr.s_addr = host;
	WIN(_address.sin_addr.S_un.S_addr = host;)
	NIX(_address.sin_addr.s_addr = host;)

	_address.sin_port = htons(port);

	if (connect(clientSocket, (sockaddr *)&_address, sizeof(_address))
			WIN(== SOCKET_ERROR) NIX(!= 0))
	{
		WIN(closesocket(clientSocket);)
		NIX(close(clientSocket);)
		return false;
	}

	if (!enableKeepAlive(clientSocket))
	{
		shutdown(clientSocket, 0);
		WIN(closesocket)
		NIX(close)
		(clientSocket);
	}

	std::unique_ptr<Client> client(new Client(clientSocket, _address));
	connect_hndl(*client);
	clientMutex.lock();
	clients.push_back(std::move(client));
	clientMutex.unlock();
	return true;
}

void TcpServer::sendData(const char *buffer, const u64 size)
{
	clientMutex.lock();
	for (auto &client : clients)
		client->sendData(buffer, size);
	clientMutex.unlock();
}

bool TcpServer::sendDataBy(u32 host, u16 port, const char *buffer, const u64 size)
{
	bool data_is_sended = false;
	clientMutex.lock();
	for (auto &client : clients)
		if (client->getHost() == host &&
			client->getPort() == port)
		{
			client->sendData(buffer, size);
			data_is_sended = true;
		}
	clientMutex.unlock();
	return data_is_sended;
}

bool TcpServer::disconnectBy(u32 host, u16 port)
{
	bool client_is_disconnected = false;
	clientMutex.lock();
	for (auto &client : clients)
		if (client->getHost() == host &&
			client->getPort() == port)
		{
			client->disconnect();
			client_is_disconnected = true;
		}
	clientMutex.unlock();
	return client_is_disconnected;
}

void TcpServer::disconnectAll()
{
	clientMutex.lock();
	for (auto &client : clients)
		client->disconnect();
	clientMutex.unlock();
}

string SocketTCP::TcpServer::explainStatus()
{
	i32 lastError{0};
	WIN(if (_status != status::up and _status != status::close) {
		lastError = WSAGetLastError();
	})
	switch (_status)
	{
	case SocketTCP::TcpServer::status::up:
		return "Socket is up.";
	case SocketTCP::TcpServer::status::err_socket_init:
		return std::format("Error: couldn't initialize a socket ({}).", lastError);
	case SocketTCP::TcpServer::status::err_socket_bind:
		return std::format("Error: couldn't bind a socket ({}).", lastError);
	case SocketTCP::TcpServer::status::err_socket_keep_alive:
		return std::format("Error: couldn't keep a socket alive ({}).", lastError);
	case SocketTCP::TcpServer::status::err_socket_listening:
		return std::format("Error: couldn't std::listen a socket ({}).", lastError);
	case SocketTCP::TcpServer::status::close:
		return "Socket is closed.";
	default:
		return "Unknown state.";
	}
}

void TcpServer::handlingAcceptLoop()
{
	if (halts)
		return;
	SockLen_t addrlen = sizeof(SocketAddr_in);
	SocketAddr_in clientAddr{};
	if (Socket clientSocket =
			WIN(accept(serv_socket, (struct sockaddr *)&clientAddr, &addrlen))
				NIX(accept4(serv_socket, (struct sockaddr *)&clientAddr, &addrlen, SOCK_NONBLOCK));
		clientSocket WIN(!= 0) NIX(>= 0) && _status == status::up)
	{
		if (enableKeepAlive(clientSocket))
		{
			std::unique_ptr<Client> client(new Client(clientSocket, clientAddr));
			connect_hndl(*client);
			clientMutex.lock();
			clients.push_back(std::move(client));
			clientMutex.unlock();
		}
		else
		{
			shutdown(clientSocket, 0);
			WIN(closesocket)
			NIX(close)
			(clientSocket);
		}
	}

	if (_status == status::up and not halts)
		_threads.addJob([this]()
						{ handlingAcceptLoop(); });
}

void TcpServer::waitingDataLoop()
{
	if (halts)
		return;
	clientMutex.lock();
	ClientIterator iter = clients.begin();
	for (auto &client : clients)
	{
		if (not client)
			continue;
		client->accessMutex.lock();
		if (DataBuffer data = client->loadData(); !data.empty())
		{
			if (client->_answered)
			{
				client->accessMutex.unlock();
				continue;
			}
			client->_answered = true;
			_threads.addJob([this, _data = std::move(data), &client]
							{
					clientMutex.lock();
					if (not client)
					{
						clientMutex.unlock();
						return;
					}
					client->accessMutex.lock();
					handler(_data, *client);
					client->_answered = false;
					client->accessMutex.unlock();
					clientMutex.unlock(); });
		}
		else if (client->_status == SocketStatus::disconnected and not client->_removed)
		{
			client->_removed = true;
			_threads.addJob([this, &client, iter]
							{
					clientMutex.lock();
					if (not client)
					{
						clientMutex.unlock();
						return;
					}
					client->accessMutex.lock();
					disconnect_hndl(*client);
					client->accessMutex.unlock();
					client.reset();
					clients.erase(iter);
					clientMutex.unlock(); });
		}
		client->accessMutex.unlock();
		++iter;
	}
	clientMutex.unlock();
	if (_status == status::up and not halts)
		_threads.addJob([this]()
						{ waitingDataLoop(); });
}

bool TcpServer::enableKeepAlive(Socket socket)
{
	i32 flag = 1;
#ifdef _WIN32
	tcp_keepalive ka{1, (u_long)ka_conf.ka_idle * 1000, (u_long)ka_conf.ka_intvl * 1000};
	if (setsockopt(socket, SOL_SOCKET, SO_KEEPALIVE, (const char *)&flag, sizeof(flag)) != 0)
		return false;
	u_long numBytesReturned = 0;
	if (WSAIoctl(socket, SIO_KEEPALIVE_VALS, &ka, sizeof(ka), nullptr, 0, &numBytesReturned, 0, nullptr) != 0)
		return false;
#else
	if (setsockopt(socket, SOL_SOCKET, SO_KEEPALIVE, &flag, sizeof(flag)) == -1)
		return false;
	if (setsockopt(socket, IPPROTO_TCP, TCP_KEEPIDLE, &ka_conf.ka_idle, sizeof(ka_conf.ka_idle)) == -1)
		return false;
	if (setsockopt(socket, IPPROTO_TCP, TCP_KEEPINTVL, &ka_conf.ka_intvl, sizeof(ka_conf.ka_intvl)) == -1)
		return false;
	if (setsockopt(socket, IPPROTO_TCP, TCP_KEEPCNT, &ka_conf.ka_cnt, sizeof(ka_conf.ka_cnt)) == -1)
		return false;
#endif
	return true;
}

string SocketTCP::getHostStr(const TcpServer::Client &client)
{
	u32 ip = client.getHost();
	return U32ToIpAddress(ip) + ':' + std::to_string(client.getPort());
}