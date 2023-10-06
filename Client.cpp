#include "Client.hpp"

using namespace SocketTCP;

void SocketTCP::Client::Client::handle()
{
	try
	{
		while (status_ == ClientSocketStatus::kConnected)
		{
			DataBuffer data = loadData();
			if (not data.empty())
			{
				std::scoped_lock lock(handle_mutex_);
				handler_(std::move(data));
			}
		}
	}
	catch (std::exception &exception)
	{
		std::cout
			<< std::format("\033[91mSome unexpected exception has happened:\n{}\n\033[0m", exception.what())
			<< "\033[93mPlease, contact the developer about this issue.\033[0m"
			<< std::endl;
	}
}

Client::Client::Client() noexcept : status_(ClientSocketStatus::kDisconnected),
									address_(),
									socket_(),
									timeout_(1)
{
	handler_ = [](DataBuffer) {};
}

Client::Client::~Client()
{
	disconnect();
}

ClientSocketStatus SocketTCP::Client::Client::connectTo(u32 host, u16 port) noexcept
{
	socket_ = sockets->NewSocket(AF_INET, SOCK_STREAM, IPPROTO_IP);
	if (socket_ WIN(== INVALID_SOCKET) NIX(< 0))
		return status_ = ClientSocketStatus::kErrSocketInit;

#ifdef _WIN32
	u32 tv = timeout_ * 1000;
	sockets->SetSockOptions(socket_, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<char *>(&tv), sizeof(tv));
#else
	struct timeval tv {timeout_, 0};
	sockets->SetSockOptions(socket_, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<char *>(&tv), sizeof(tv));
#endif

	new (&address_) SocketAddr_in;
	address_.sin_family = AF_INET;
	address_.sin_addr.s_addr = host;
	address_.sin_port = htons(port);

	if (sockets->Connect(socket_, address_) WIN(== SOCKET_ERROR) NIX(!= 0))
	{
		sockets->Close(socket_);
		return status_ = ClientSocketStatus::kErrSocketConnect;
	}
	return status_ = ClientSocketStatus::kConnected;
}

ClientSocketStatus SocketTCP::Client::Client::disconnect() noexcept
{
	if (status_ != ClientSocketStatus::kConnected)
		return status_;
	status_ = ClientSocketStatus::kDisconnected;
	sockets->Shutdown(socket_, SD_BOTH);
	sockets->Close(socket_);
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

	size = *reinterpret_cast<u64*>(sizeBuffer.data());

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

bool SocketTCP::Client::Client::setHandler(HandlerFunctionType handler)
{
	std::scoped_lock lock(handle_mutex_);
	handler_ = handler;
	thread = std::jthread(&Client::handle, this);
	return true;
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

	if (sockets->Send(socket_, send_buffer, 0) < 0)
		return false;

	return true;
}
