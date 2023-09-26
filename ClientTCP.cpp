#include "ClientTCP.hpp"

using namespace SocketTCP;

void SocketTCP::ClientTCP::handleSingleThread()
{
	try {
		while (_status == SocketStatus::connected) {
			if (DataBuffer data = loadData(); !data.empty()) {
				std::scoped_lock lock(_handleMutex);
				_handler(std::move(data));
			}
			else if (_status != SocketStatus::connected) return;
		}
	}
	catch (std::exception& exception) {
		std::cout
			<< std::format("\033[91mSome unexpected exception has happened:\n{}\n\033[0m", exception.what())
			<< "\033[93mPlease, contact the developer about this issue.\033[0m"
			<< std::endl;
		return;
	}
}

void SocketTCP::ClientTCP::handleThreadPool()
{
	try {
		if (DataBuffer data = loadData(); !data.empty()) {
			std::scoped_lock lock(_handleMutex);
			_handler(std::move(data));
		}
		if (_status == SocketStatus::connected) _threads.thread_pool->addJob([this] { handleThreadPool(); });
	}
	catch (std::exception& exception) {
		std::cout
			<< std::format("\033[91mSome unexpected exception has happened:\n{}\n\033[0m", exception.what())
			<< "\033[93mPlease, contact the developer about this issue.\033[0m"
			<< std::endl;
		return;
	}
	catch (...) {
		std::cout << "Unhandled exception!" << std::endl;
		return;
	}
}

ClientTCP::ClientTCP() noexcept :
	_status(SocketStatus::disconnected),
	_threadManagementType(ThreadManagementType::single_thread),
	_address(),
	_socket()
{}

ClientTCP::ClientTCP(ThreadPool* thread_pool) noexcept :
	_threadManagementType(ThreadManagementType::thread_pool),
	_threads(thread_pool),
	_status(SocketStatus::disconnected),
	_address(),
	_socket()
{}

ClientTCP::~ClientTCP() {
	disconnect();
	WIN(WSACleanup();)
		switch (_threadManagementType) {
		case ThreadManagementType::single_thread:
			if (_threads.thread) _threads.thread->join();
			delete _threads.thread;
			break;
		case ThreadManagementType::thread_pool: break;
		}
}

SocketStatus SocketTCP::ClientTCP::connectTo(u32 host, u16 port) noexcept
{
	if ((_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_IP)) WIN(== INVALID_SOCKET) NIX(< 0))
		return _status = SocketStatus::err_socket_init;

	new(&_address) SocketAddr_in;
	_address.sin_family = AF_INET;
	_address.sin_addr.s_addr = host;
	_address.sin_port = htons(port);

	if (connect(_socket, (sockaddr*)&_address, sizeof(_address))
		WIN(== SOCKET_ERROR)NIX(!= 0)
		)
	{
		WIN(closesocket(_socket))
			NIX(close(_socket));
		return _status = SocketStatus::err_socket_connect;
	}
	return _status = SocketStatus::connected;
}

SocketStatus SocketTCP::ClientTCP::disconnect() noexcept
{
	if (_status != SocketStatus::connected)
		return _status;
	_status = SocketStatus::disconnected;
	switch (_threadManagementType) {
	case ThreadManagementType::single_thread:
		if (_threads.thread) _threads.thread->join();
		delete _threads.thread;
		break;
	case ThreadManagementType::thread_pool: break;
	}
	shutdown(_socket, SD_BOTH);
	WIN(closesocket(_socket))
		NIX(close(_socket));
	return _status;
}

u32 SocketTCP::ClientTCP::getHost() const
{
	return _address.sin_addr.s_addr;
}

u16 SocketTCP::ClientTCP::getPort() const
{
	return _address.sin_port;
}

DataBuffer SocketTCP::ClientTCP::loadData()
{
	DataBuffer buffer;
	u64 size{ 0 };
	u64 err{ 0 };

	WIN(if (u_long t = true; SOCKET_ERROR == ioctlsocket(_socket, FIONBIO, &t)) return DataBuffer();)
	u64 answ = recv(_socket, (char*)&size, sizeof(size), NIX(MSG_DONTWAIT)WIN(0));
	WIN(if (u_long t = false; SOCKET_ERROR == ioctlsocket(_socket, FIONBIO, &t)) return DataBuffer();)

		if (!answ) {
			disconnect();
			return DataBuffer();
		}
		else if (answ == -1) {
			WIN(
				err = convertError();
			if (!err) {
				SockLen_t len = sizeof(err);
				getsockopt(_socket, SOL_SOCKET, SO_ERROR, WIN((char*)) & err, &len);
			}
			)NIX(
				SockLen_t len = sizeof(err);
			getsockopt(_socket, SOL_SOCKET, SO_ERROR, & err, &len);
			if (!err) err = errno;
			)

				switch (err) {
				case 0: break;
				case ETIMEDOUT:
				case ECONNRESET:
				case EPIPE:
					disconnect();
					[[fallthrough]];
				case EAGAIN: return DataBuffer();
				default:
					disconnect();
					return DataBuffer();
				}
		}

	if (!size) return DataBuffer();
	buffer.resize(size);
	recv(_socket, (char*)buffer.data(), (i32)buffer.size(), 0);
	return buffer;
}

DataBuffer SocketTCP::ClientTCP::loadDataSync()
{
	DataBuffer data;
	u64 size = 0;
	u64 answ = recv(_socket, reinterpret_cast<char*>(&size), sizeof(size), 0);
	if (size && answ == sizeof(size)) {
		data.resize(size);
		recv(_socket, reinterpret_cast<char*>(data.data()), (i32)data.size(), 0);
	}
	return data;
}

void SocketTCP::ClientTCP::setHandler(handlerFunctionType handler)
{
	std::scoped_lock lock(_handleMutex);
	_handler = handler;

	switch (_threadManagementType) {
	case ThreadManagementType::single_thread:
		if (_threads.thread) return;
		_threads.thread = new std::jthread(&ClientTCP::handleSingleThread, this);
		break;
	case ThreadManagementType::thread_pool:
		_threads.thread_pool->addJob([this] {handleThreadPool(); });
		break;
	}
}

void SocketTCP::ClientTCP::joinHandler()
{
	switch (_threadManagementType) {
	case ThreadManagementType::single_thread:
		if (_threads.thread) _threads.thread->join();
		break;
	case ThreadManagementType::thread_pool:
		_threads.thread_pool->join();
		break;
	}
}

bool SocketTCP::ClientTCP::sendData(const char* buffer, const u64 size) const
{
	void* sendBuffer = malloc(size + sizeof(u64));
	if (sendBuffer == nullptr)
		return false;
	memcpy(reinterpret_cast<char*>(sendBuffer) + sizeof(u64), buffer, size);
	*reinterpret_cast<u64*>(sendBuffer) = size;
	if (send(_socket, reinterpret_cast<char*>(sendBuffer), (i32)(size + sizeof(u64)), 0) < 0)
		return false;
	free(sendBuffer);
	return true;
}
