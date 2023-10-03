#include "Server.hpp"

using namespace SocketTCP;

DataBuffer Server::Server::Client::loadData()
{
	if (status_ != ClientSocketStatus::kConnected)
		return DataBuffer();

	DataBuffer result;
	u64 size = 0;

	WIN(if (u_long t = 1; SOCKET_ERROR == ioctlsocket(socket_, FIONBIO, &t)) return DataBuffer();)

	i32 answer = recv(socket_, reinterpret_cast<char *>(&size), sizeof(size), NIX(MSG_DONTWAIT) WIN(0));
	RecvResult check = checkRecv(answer);

	WIN(if (u_long t = 0; SOCKET_ERROR == ioctlsocket(socket_, FIONBIO, &t)) return DataBuffer();)

	switch (check)
	{
	case SocketTCP::RecvResult::kOk:
		break;
	case SocketTCP::RecvResult::kAgain:
		return DataBuffer();
	case SocketTCP::RecvResult::kDisconnect:
		disconnect();
		return DataBuffer();
	default:
		break;
	}

	if (!size)
		return DataBuffer();

	result.resize(size);

	WIN(if (u_long t = 1; SOCKET_ERROR == ioctlsocket(socket_, FIONBIO, &t)) return DataBuffer();)

	answer = recv(socket_, reinterpret_cast<char *>(result.data()), static_cast<i32>(size), NIX(MSG_DONTWAIT) WIN(0));
	check = checkRecv(answer);

	WIN(if (u_long t = 0; SOCKET_ERROR == ioctlsocket(socket_, FIONBIO, &t)) return DataBuffer();)

	switch (check)
	{
	case SocketTCP::RecvResult::kOk:
		break;
	case SocketTCP::RecvResult::kAgain:
		return DataBuffer();
	case SocketTCP::RecvResult::kDisconnect:
		disconnect();
		return DataBuffer();
	default:
		break;
	}

	return result;
}

RecvResult SocketTCP::Server::Server::Client::checkRecv(i32 answer)
{
	i64 err = 0;

	if (!answer)
	{
		disconnect();
		return RecvResult::kDisconnect;
	}
	else if (answer == -1)
	{
		WIN(
			err = convertError();
			if (!err) {
				SockLen_t len = sizeof(err);
				getsockopt(socket_, SOL_SOCKET, SO_ERROR, reinterpret_cast<char *>(&err), &len);
			})
		NIX(
			SockLen_t len = sizeof(err);
			getsockopt(socket_, SOL_SOCKET, SO_ERROR, &err, &len);
			if (!err) err = errno;)
		switch (err)
		{
		case 0:
			break;
		case ETIMEDOUT:
		case ECONNRESET:
		case EPIPE:
			return RecvResult::kDisconnect;
		case EAGAIN:
			return RecvResult::kAgain;
		default:
			return RecvResult::kDisconnect;
		}
	}
	return RecvResult::kOk;
}

ClientSocketStatus Server::Server::Client::disconnect()
{
	status_ = ClientSocketStatus::kDisconnected;
	if (socket_ == WIN(INVALID_SOCKET) NIX(-1))
		return status_;
	shutdown(socket_, SD_BOTH);
	WIN(closesocket)
	NIX(close)
	(socket_);
	socket_ = WIN(INVALID_SOCKET) NIX(-1);
	return status_;
}

bool Server::Server::Client::sendData(const string &data) const
{
	if (status_ != ClientSocketStatus::kConnected)
		return false;

	DataBuffer send_buffer = DataBuffer();
	u64 size = data.size() + 1;
	for (u64 i = 0; i < sizeof(u64); ++i)
	{
		send_buffer.push_back(static_cast<u8>(size & 0xFF));
		size >>= 8;
	}
	std::copy(data.begin(), data.end(), std::back_inserter(send_buffer));
	send_buffer.push_back(0);

	if (send(socket_, reinterpret_cast<char *>(send_buffer.data()), static_cast<i32>(send_buffer.size()), 0) < 0)
		return false;

	return true;
}

Server::Server::Client::Client(Socket socket, SocketAddr_in address) : address_(address), socket_(socket) {}

Server::Server::Client::~Client()
{
	if (socket_ == WIN(INVALID_SOCKET) NIX(-1))
		return;
	shutdown(socket_, SD_BOTH);
	WIN(closesocket(socket_);)
	NIX(close(socket_);)
}

u32 Server::Server::Client::getHost() const { return address_.sin_addr.s_addr; }
u16 Server::Server::Client::getPort() const { return address_.sin_port; }

Server::Server::Server(const u32 ip_address, const u16 port,
					   KeepAliveConfig conf,
					   HandlerFunctionType handler,
					   ConnectionHandlerFunctionType connect_handler_,
					   ConnectionHandlerFunctionType disconnect_handler_,
					   u64 thread_count)
	: ip_address_(ip_address),
	  port_(port),
	  handler(handler),
	  connect_handler_(connect_handler_),
	  disconnect_handler_(disconnect_handler_),
	  thread(thread_count),
	  ka_config(conf),
	  socket_(0),
	  timeout_(1)
{
}

Server::Server::~Server()
{
	if (status_ == Status::kUp)
		stop();
}

void Server::Server::setHandler(Server::HandlerFunctionType handler)
{
	this->handler = handler;
}

u16 Server::Server::getPort() const { return port_; }
u16 Server::Server::setPort(const u16 port)
{
	this->port_ = port;
	start();
	return port;
}

u32 Server::Server::getIpAddress() const { return ip_address_; }
u32 Server::Server::setIpAddress(const u32 ip_address)
{
	this->ip_address_ = ip_address;
	start();
	return ip_address;
}

Server::Server::Status Server::Server::start()
{
	i32 flag = true;
	if (status_ == Status::kUp)
		stop();

	SocketAddr_in address_{};
	address_.sin_addr.s_addr = ip_address_;
	address_.sin_port = htons(port_);
	address_.sin_family = AF_INET;

	if ((socket_ = socket(AF_INET, SOCK_STREAM NIX(| SOCK_NONBLOCK), 0)) WIN(== INVALID_SOCKET) NIX(== -1))
		return status_ = Status::kErrSocketInit;

#ifdef _WIN32
	u32 timeout = timeout_ * 1000;
	setsockopt(socket_, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<char *>(&timeout), sizeof(timeout));
#else
	struct timeval tv
	{
		timeout_, 0
	};
	setsockopt(socket_, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<char *>(&tv), sizeof(tv));
#endif

	WIN(
		if (u_long mode = 0; ioctlsocket(socket_, FIONBIO, &mode) == SOCKET_ERROR) {
			return status_ = Status::kErrSocketInit;
		})

	if ((setsockopt(socket_, SOL_SOCKET, SO_REUSEADDR, WIN(reinterpret_cast<char *>)(&flag), sizeof(flag)) == -1) ||
		(bind(socket_, reinterpret_cast<struct sockaddr *>(&address_), sizeof(address_)) WIN(== SOCKET_ERROR) NIX(< 0)))
		return status_ = Status::kErrSocketBind;

	if (listen(socket_, SOMAXCONN) WIN(== SOCKET_ERROR) NIX(< 0))
		return status_ = Status::kErrSocketListening;
	status_ = Status::kUp;
	thread.addJob([this]
				  { handlingAcceptLoop(); });
	thread.addJob([this]
				  { waitingDataLoop(); });
	return status_;
}

void Server::Server::stop()
{
	thread.dropUnstartedJobs();
	status_ = Status::kClose;
	WIN(closesocket)
	NIX(close)
	(socket_);
	client_work_mutex_.lock();
	clients_.clear();
	client_work_mutex_.unlock();
}

void Server::Server::joinLoop()
{
	thread.join();
}

void Server::Server::halt()
{
	halts_ = true;
	thread.halt();
}

bool Server::Server::connectTo(u32 host, u16 port, ConnectionHandlerFunctionType connect_handler_)
{
	Socket clientSocket;
	SocketAddr_in address_{};
	if ((clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_IP)) WIN(== INVALID_SOCKET) NIX(< 0))
		return false;

	new (&address_) SocketAddr_in;
	address_.sin_family = AF_INET;
	address_.sin_addr.s_addr = host;
	WIN(address_.sin_addr.S_un.S_addr = host;)
	NIX(address_.sin_addr.s_addr = host;)

	address_.sin_port = htons(port);

	if (connect(clientSocket, (sockaddr *)&address_, sizeof(address_))
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

	std::unique_ptr<Client> client(new Client(clientSocket, address_));
	connect_handler_(*client);
	client_work_mutex_.lock();
	clients_.push_back(std::move(client));
	client_work_mutex_.unlock();
	return true;
}

void Server::Server::sendData(const string &buffer)
{
	client_work_mutex_.lock();
	for (auto &client : clients_)
		client->sendData(buffer);
	client_work_mutex_.unlock();
}

bool Server::Server::sendDataBy(u32 host, u16 port, const string &data)
{
	bool data_sent = false;
	client_work_mutex_.lock();
	for (auto &client : clients_)
		if (client->getHost() == host && client->getPort() == port)
		{
			client->sendData(data);
			data_sent = true;
			break;
		}
	client_work_mutex_.unlock();
	return data_sent;
}

bool Server::Server::disconnectBy(u32 host, u16 port)
{
	bool client_is_disconnected = false;
	client_work_mutex_.lock();
	for (auto &client : clients_)
		if (client->getHost() == host && client->getPort() == port)
		{
			client->disconnect();
			client_is_disconnected = true;
			break;
		}
	client_work_mutex_.unlock();
	return client_is_disconnected;
}

void Server::Server::disconnectAll()
{
	client_work_mutex_.lock();
	for (auto &client : clients_)
		client->disconnect();
	client_work_mutex_.unlock();
}

string Server::Server::explainStatus()
{
	i32 lastError{0};
	WIN(if (status_ != Status::kUp and status_ != Status::kClose) {
		lastError = WSAGetLastError();
	})
	switch (status_)
	{
	case Server::Server::Status::kUp:
		return "Socket is up.";
	case Server::Server::Status::kErrSocketInit:
		return std::format("Error: couldn't initialize a socket ({}).", lastError);
	case Server::Server::Status::kErrSocketBind:
		return std::format("Error: couldn't bind a socket ({}).", lastError);
	case Server::Server::Status::kErrSocketKeepAlive:
		return std::format("Error: couldn't keep a socket alive ({}).", lastError);
	case Server::Server::Status::kErrSocketListening:
		return std::format("Error: couldn't std::listen a socket ({}).", lastError);
	case Server::Server::Status::kClose:
		return "Socket is closed.";
	default:
		return "Unknown state.";
	}
}

void Server::Server::handlingAcceptLoop()
{
	if (halts_)
		return;
	SockLen_t addrlen = sizeof(SocketAddr_in);
	SocketAddr_in clientAddr{};
	Socket clientSocket{};

	timeval waiting_time_tv{0, 5000};

	fd_set master{};
	FD_ZERO(&master);
	FD_SET(socket_, &master);

	fd_set temp = master;
	if (select(WIN(NULL)NIX(socket_ + 1), &temp, NULL, NULL, &waiting_time_tv) == -1)
	{
		if (status_ == Status::kUp and not halts_)
			thread.addJob([this]()
						  { handlingAcceptLoop(); });
		return;
	}

	if (FD_ISSET(socket_, &temp))
		clientSocket =
			WIN(accept(socket_, reinterpret_cast<struct sockaddr *>(&clientAddr), &addrlen))
				NIX(accept4(socket_, reinterpret_cast<struct sockaddr *>(&clientAddr), &addrlen, SOCK_NONBLOCK));
	else
	{
		if (status_ == Status::kUp and not halts_)
			thread.addJob([this]()
						  { handlingAcceptLoop(); });
		return;
	}

	if (clientSocket WIN(!= 0) NIX(>= 0) && status_ == Status::kUp)
	{
		if (enableKeepAlive(clientSocket))
		{
			std::unique_ptr<Client> client(new Client(clientSocket, clientAddr));
			connect_handler_(*client);
			client_work_mutex_.lock();
			clients_.push_back(std::move(client));
			client_work_mutex_.unlock();
		}
		else
		{
			shutdown(clientSocket, 0);
			WIN(closesocket)
			NIX(close)
			(clientSocket);
		}
	}

	if (status_ == Status::kUp and not halts_)
		thread.addJob([this]()
					  { handlingAcceptLoop(); });
}

void Server::Server::waitingDataLoop()
{
	if (halts_)
		return;
	client_work_mutex_.lock();
	ClientIterator iter = clients_.begin();
	for (auto &client : clients_)
	{
		if (not client)
			continue;
		client->access_mutex_.lock();
		if (DataBuffer data = client->loadData(); !data.empty())
		{
			if (client->answered_)
			{
				client->access_mutex_.unlock();
				continue;
			}
			client->answered_ = true;
			thread.addJob([this, data = std::move(data), &client]
						  {
					client_work_mutex_.lock();
					if (not client)
					{
						client_work_mutex_.unlock();
						return;
					}
					client->access_mutex_.lock();
					handler(data, *client);
					client->answered_ = false;
					client->access_mutex_.unlock();
					client_work_mutex_.unlock(); });
		}
		else if (client->status_ == ClientSocketStatus::kDisconnected and not client->removed_)
		{
			client->removed_ = true;
			thread.addJob([this, &client, iter]
						  {
					client_work_mutex_.lock();
					if (not client)
					{
						client_work_mutex_.unlock();
						return;
					}
					client->access_mutex_.lock();
					disconnect_handler_(*client);
					client->access_mutex_.unlock();
					client.reset();
					clients_.erase(iter);
					client_work_mutex_.unlock(); });
		}
		client->access_mutex_.unlock();
		++iter;
	}
	client_work_mutex_.unlock();
	if (status_ == Status::kUp and not halts_)
		thread.addJob([this]()
					  { waitingDataLoop(); });
}

bool Server::Server::enableKeepAlive(Socket socket_)
{
	i32 flag = 1;
#ifdef _WIN32
	tcp_keepalive ka{1, static_cast<u_long>(ka_config.idle) * 1000, static_cast<u_long>(ka_config.intvl) * 1000};
	if (setsockopt(socket_, SOL_SOCKET, SO_KEEPALIVE, reinterpret_cast<char *>(&flag), sizeof(flag)) != 0)
		return false;
	u_long numBytesReturned = 0;
	if (WSAIoctl(socket_, SIO_KEEPALIVE_VALS, &ka, sizeof(ka), nullptr, 0, &numBytesReturned, 0, nullptr) != 0)
		return false;
#else
	if (setsockopt(socket_, SOL_SOCKET, SO_KEEPALIVE, &flag, sizeof(flag)) == -1)
		return false;
	if (setsockopt(socket_, IPPROTO_TCP, TCP_KEEPIDLE, &ka_config.idle, sizeof(ka_config.idle)) == -1)
		return false;
	if (setsockopt(socket_, IPPROTO_TCP, TCP_KEEPINTVL, &ka_config.intvl, sizeof(ka_config.intvl)) == -1)
		return false;
	if (setsockopt(socket_, IPPROTO_TCP, TCP_KEEPCNT, &ka_config.cnt, sizeof(ka_config.cnt)) == -1)
		return false;
#endif
	return true;
}

string SocketTCP::Server::getHostStr(const Server::Client &client)
{
	u32 ip = client.getHost();
	return U32ToIpAddress(ip) + ':' + std::to_string(client.getPort());
}