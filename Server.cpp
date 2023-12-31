#include "Server.hpp"

using namespace SocketTCP;

DataBuffer Server::Server::Client::loadData() {
	DataBuffer result;
	DataBuffer sizeBuffer = {0, 0, 0, 0, 0, 0, 0, 0};
	u64 size = 0;

	RecvResult check = sockets->Recv(socket_, &sizeBuffer, sizeof(size), NIX(MSG_DONTWAIT) WIN(0));

	switch (check)
	{
	case SocketTCP::RecvResult::kAgain:
		return DataBuffer();
	case SocketTCP::RecvResult::kDisconnect:
		disconnect();
		return DataBuffer();
	default:
		break;
	}

	size = *reinterpret_cast<u64 *>(sizeBuffer.data());

	if (size == 0)
		return DataBuffer();

	check = sockets->Recv(socket_, &result, size, NIX(MSG_DONTWAIT) WIN(0));

	switch (check)
	{
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

void Server::Server::Client::disconnect() {
	if (socket_ != WIN(INVALID_SOCKET) NIX(-1))
	{
		sockets->Shutdown(socket_, SD_BOTH);
		sockets->Close(socket_);
		socket_ = WIN(INVALID_SOCKET) NIX(-1);
	}
	disconnected_ = true;
}

bool Server::Server::Client::sendData(const string &data) const {
	DataBuffer send_buffer = DataBuffer();
	u64 size = data.size() + 1;
	for (u64 i = 0; i < sizeof(u64); ++i)
	{
		send_buffer.push_back(static_cast<u8>(size & 0xFF));
		size >>= 8;
	}
	std::copy(data.begin(), data.end(), std::back_inserter(send_buffer));
	send_buffer.push_back(0);

	if (sockets->Send(socket_, send_buffer, 0) < 0)
		return false;

	return true;
}

Server::Server::Client::Client(Socket socket) : socket_(socket) {}

Server::Server::Client::~Client() {
	if (socket_ != WIN(INVALID_SOCKET) NIX(-1))
	{
		sockets->Shutdown(socket_, SD_BOTH);
		sockets->Close(socket_);
	}
}

Server::Server::Server(const i32 ip_address, const u16 port,
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

Server::Server::~Server() {
	if (status_ == Status::kUp)
		stop();
}

void Server::Server::setHandler(Server::HandlerFunctionType handler) {
	this->handler = handler;
}

u16 Server::Server::getPort() const { return port_; }
u16 Server::Server::setPort(const u16 port) {
	this->port_ = port;
	start();
	return port;
}

u32 Server::Server::getIpAddress() const { return ip_address_; }
u32 Server::Server::setIpAddress(const u32 ip_address) {
	this->ip_address_ = ip_address;
	start();
	return ip_address;
}

Server::Server::Status Server::Server::start() {
	if (status_ == Status::kUp)
		stop();

	SocketAddr_in address_{};
	address_.sin_addr.s_addr = ip_address_;
	address_.sin_port = htons(port_);
	address_.sin_family = AF_INET;

	socket_ = sockets->NewSocket(AF_INET, SOCK_STREAM NIX(| SOCK_NONBLOCK), 0);

	if (socket_ WIN(== INVALID_SOCKET) NIX(== -1))
		return status_ = Status::kErrSocketInit;

#ifdef _WIN32
	u32 timeout = timeout_ * 1000;
	sockets->SetSockOptions(socket_, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<char *>(&timeout), sizeof(timeout));

	if (u32 mode = 0; sockets->IOctlSocket(socket_, FIONBIO, &mode) == SOCKET_ERROR)
		return status_ = Status::kErrSocketInit;
#else
	struct timeval tv
	{
		timeout_, 0
	};
	sockets->SetSockOptions(socket_, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<char *>(&tv), sizeof(tv));
#endif

	i32 flag = true;
	if ((sockets->SetSockOptions(socket_, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<char *>(&flag), sizeof(flag)) == -1) or
		(sockets->Bind(socket_, address_) < 0))
		return status_ = Status::kErrSocketBind;

	if (sockets->Listen(socket_, SOMAXCONN) < 0)
		return status_ = Status::kErrSocketListening;

	halts_ = false;
	thread.addJob([this]
				  { handlingAcceptLoop(); });
	thread.addJob([this]
				  { waitingDataLoop(); });
	return status_ = Status::kUp;
}

void Server::Server::stop() {
	halt();
	thread.dropUnstartedJobs();
	status_ = Status::kClose;
	sockets->Close(socket_);
	client_work_mutex_.lock();
	clients_.clear();
	client_work_mutex_.unlock();
}

void Server::Server::joinLoop() {
	thread.join();
}

void Server::Server::halt() {
	halts_ = true;
	thread.halt();
}

string Server::Server::explainStatus() {
	i32 lastError{0};
#ifdef _WIN32
	if (status_ != Status::kUp and status_ != Status::kClose)
	{
		lastError = WSAGetLastError();
	}
#endif
	switch (status_)
	{
	case Server::Server::Status::kUp:
		return "Socket is up.";
	case Server::Server::Status::kErrSocketInit:
		return std::format("Error: couldn't initialize a socket ({}).", lastError);
	case Server::Server::Status::kErrSocketBind:
		return std::format("Error: couldn't bind a socket ({}).", lastError);
	case Server::Server::Status::kErrSocketListening:
		return std::format("Error: couldn't listen a socket ({}).", lastError);
	default:
		return "Socket is closed.";
	}
}

void Server::Server::handlingAcceptLoop() {
	timeval waiting_time_tv{0, 5000};

	fd_set master{};
	FD_ZERO(&master);
	sockets->FdSet(socket_, &master);

	fd_set temp = master;
	if (sockets->SelectBeforeAccept(socket_, &temp, &waiting_time_tv) != -1 and sockets->FdIsSet(socket_, &temp))
	{
		SocketAddr_in clientAddr{};
		Socket clientSocket = sockets->Accept(socket_, clientAddr);
		if (clientSocket WIN(!= 0) NIX(>= 0))
		{
			if (enableKeepAlive(clientSocket))
			{
				std::unique_ptr<Client> client(new Client(clientSocket));
				client->sockets = sockets;
				connect_handler_(*client);
				client_work_mutex_.lock();
				clients_.push_back(std::move(client));
				client_work_mutex_.unlock();
			}
			else
			{
				sockets->Shutdown(clientSocket, 0);
				sockets->Close(clientSocket);
			}
		}
	}

	if (not halts_)
		thread.addJob([this]()
					  { handlingAcceptLoop(); });
}

void Server::Server::waitingDataLoop() {
	DataBuffer data;
	client_work_mutex_.lock();
	ClientIterator iter = clients_.begin();
	for (auto &client : clients_)
	{
		client->access_mutex_.lock();
		if (not client->disconnected_ and not client->removed_)
			data = client->loadData();
		else
			data.clear();
		if (!data.empty())
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
					if (client == nullptr)
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
		else if (client->disconnected_ and not client->removed_)
		{
			client->removed_ = true;
			thread.addJob([this, &client, iter]
						  {
					client_work_mutex_.lock();
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

	if (not halts_)
		thread.addJob([this]()
					  { waitingDataLoop(); });
}

bool Server::Server::enableKeepAlive(Socket socket_) {
	i32 flag = 1;
#ifdef _WIN32
	tcp_keepalive ka{1, static_cast<u_long>(ka_config.idle) * 1000, static_cast<u_long>(ka_config.intvl) * 1000};
	if (sockets->SetSockOptions(socket_, SOL_SOCKET, SO_KEEPALIVE, reinterpret_cast<char *>(&flag), sizeof(flag)) != 0)
		return false;
	u32 numBytesReturned = 0;
	if (sockets->wsaioctl(socket_, SIO_KEEPALIVE_VALS, &ka, sizeof(ka), nullptr, 0, &numBytesReturned, 0, nullptr) != 0)
		return false;
#else
	if (sockets->SetSockOptions(socket_, SOL_SOCKET, SO_KEEPALIVE, reinterpret_cast<char *>(&flag), sizeof(flag)) == -1)
		return false;
	if (sockets->SetSockOptions(socket_, IPPROTO_TCP, TCP_KEEPIDLE, reinterpret_cast<char *>(&ka_config.idle), sizeof(ka_config.idle)) == -1)
		return false;
	if (sockets->SetSockOptions(socket_, IPPROTO_TCP, TCP_KEEPINTVL, reinterpret_cast<char *>(&ka_config.intvl), sizeof(ka_config.intvl)) == -1)
		return false;
	if (sockets->SetSockOptions(socket_, IPPROTO_TCP, TCP_KEEPCNT, reinterpret_cast<char *>(&ka_config.cnt), sizeof(ka_config.cnt)) == -1)
		return false;
#endif
	return true;
}