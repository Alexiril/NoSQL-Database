#include "Client.hpp"

using namespace SocketTCP;

void SocketTCP::Client::Client::handle()
{
	try
	{
		while (status_ == ClientSocketStatus::kConnected)
		{
			if (DataBuffer data = loadData(); !data.empty())
			{
				std::scoped_lock lock(handle_mutex_);
				_handler(std::move(data));
			}
			else if (status_ != ClientSocketStatus::kConnected)
				return;
		}
	}
	catch (std::exception &exception)
	{
		std::cout
			<< std::format("\033[91mSome unexpected exception has happened:\n{}\n\033[0m", exception.what())
			<< "\033[93mPlease, contact the developer about this issue.\033[0m"
			<< std::endl;
		return;
	}
}

Client::Client::Client() noexcept : status_(ClientSocketStatus::kDisconnected),
									address_(),
									socket_(),
									timeout_(1)
{
}

Client::Client::~Client()
{
	disconnect();
	WIN(WSACleanup();)
	if (thread.joinable())
		thread.join();
}

ClientSocketStatus SocketTCP::Client::Client::connectTo(u32 host, u16 port) noexcept
{
	if ((socket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_IP)) WIN(== INVALID_SOCKET) NIX(< 0))
		return status_ = ClientSocketStatus::kErrSocketInit;

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

	new (&address_) SocketAddr_in;
	address_.sin_family = AF_INET;
	address_.sin_addr.s_addr = host;
	address_.sin_port = htons(port);

	if (connect(socket_, (sockaddr *)&address_, sizeof(address_))
			WIN(== SOCKET_ERROR) NIX(!= 0))
	{
		WIN(closesocket(socket_))
		NIX(close(socket_));
		return status_ = ClientSocketStatus::kErrSocketConnect;
	}
	return status_ = ClientSocketStatus::kConnected;
}

ClientSocketStatus SocketTCP::Client::Client::disconnect() noexcept
{
	if (status_ != ClientSocketStatus::kConnected)
		return status_;
	status_ = ClientSocketStatus::kDisconnected;
	shutdown(socket_, SD_BOTH);
	WIN(closesocket(socket_))
	NIX(close(socket_));
	return status_;
}

u32 SocketTCP::Client::Client::getHost() const
{
	return address_.sin_addr.s_addr;
}

u16 SocketTCP::Client::Client::getPort() const
{
	return address_.sin_port;
}

DataBuffer SocketTCP::Client::Client::loadData()
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

RecvResult SocketTCP::Client::Client::checkRecv(i32 answer)
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
			std::cerr << "Server doesn't answer for too long." << std::endl;
			return RecvResult::kDisconnect;
		case ECONNRESET:
			std::cerr << "Connection has been reset." << std::endl;
			return RecvResult::kDisconnect;
		case EPIPE:
			return RecvResult::kDisconnect;
		case EAGAIN:
			return RecvResult::kAgain;
		case ENETDOWN:
			std::cerr << "Link is down. Check Internet connection." << std::endl;
			return RecvResult::kDisconnect;
		case ECONNABORTED:
			std::cerr << "Connection was aborted." << std::endl;
			return RecvResult::kDisconnect;
		default:
			std::cerr << "Unhandled error!" << std::endl
					  << "Code: " << err NIX(<< " " << strerror(errno)) << '\n';
			return RecvResult::kDisconnect;
		}
	}
	return RecvResult::kOk;
}

DataBuffer SocketTCP::Client::Client::loadDataSync()
{
	DataBuffer data;
	u64 size = 0;
	u64 answ = recv(socket_, reinterpret_cast<char *>(&size), sizeof(size), 0);
	if (size && answ == sizeof(size))
	{
		data.resize(size);
		recv(socket_, reinterpret_cast<char *>(data.data()), static_cast<i32>(data.size()), 0);
	}
	return data;
}

void SocketTCP::Client::Client::setHandler(HandlerFunctionType handler)
{
	std::scoped_lock lock(handle_mutex_);
	_handler = handler;
	thread = std::jthread(&Client::handle, this);
}

void SocketTCP::Client::Client::joinHandler()
{
	if (thread.joinable())
		thread.join();
}

bool SocketTCP::Client::Client::sendData(const string &data) const
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
