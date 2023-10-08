#include "Sockets.hpp"

using namespace SocketTCP;

Socket SocketCalls::NewSocket(i32 af, i32 type, i32 proto)
{
	return socket(af, type, proto);
}

i32 SocketTCP::SocketCalls::SetSockOptions(Socket s, i32 level, i32 opname, char* optval, u32 optsize)
{
	return setsockopt(s, level, opname, optval, optsize);
}

i32 SocketTCP::SocketCalls::Connect(Socket s, SocketAddr_in& address)
{
	return connect(s, reinterpret_cast<sockaddr*>(&address), sizeof(address));
}

i32 SocketTCP::SocketCalls::Close(Socket s)
{
	return WIN(closesocket)NIX(close)(s);
}

i32 SocketTCP::SocketCalls::Shutdown(Socket s, i32 how)
{
	return shutdown(s, how);
}

i32 SocketTCP::SocketCalls::Send(Socket s, DataBuffer& data, i32 flags)
{
	return send(s, reinterpret_cast<char*>(data.data()), static_cast<i32>(data.size()), flags);
}

i32 SocketTCP::SocketCalls::Bind(Socket s, SocketAddr_in& address)
{
	return bind(s, reinterpret_cast<sockaddr*>(&address), sizeof(address));
}

i32 SocketTCP::SocketCalls::Listen(Socket s, i32 backlog)
{
	return listen(s, backlog);
}

#ifdef _WIN32

i32 SocketTCP::SocketCalls::wsaioctl(Socket s, u32 dwIoControlCode, void* lpvInBuffer, u32 cbInBuffer, void* lpvOutBuffer, u32 cbOutBuffer, u32* lpcbBytesReturned, OVERLAPPED* lpOverlapped, LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine)
{
	return WSAIoctl(s, dwIoControlCode, lpvInBuffer, cbInBuffer, lpvOutBuffer, cbOutBuffer, reinterpret_cast<u_long*>(lpcbBytesReturned), lpOverlapped, lpCompletionRoutine);
}

i32 SocketTCP::SocketCalls::IOctlSocket(Socket s, i32 cmd, u32* argp)
{
	return ioctlsocket(s, cmd, reinterpret_cast<u_long*>(argp));
}

#endif

Socket SocketTCP::SocketCalls::Accept(Socket s, SocketAddr_in& addr)
{
	i32 size = static_cast<i32>(sizeof(addr));
#ifdef _WIN32
	return accept(s, reinterpret_cast<sockaddr*>(&addr), &size);
#else
	return accept4(s, reinterpret_cast<sockaddr*>(&addr), &size, SOCK_NONBLOCK);
#endif
}

i32 SocketTCP::SocketCalls::SelectBeforeAccept(Socket s, fd_set* readfds, const timeval* timeout)
{
	return select(WIN(0)NIX(s + 1), readfds, nullptr, nullptr, timeout);
}

RecvResult SocketTCP::SocketCalls::CheckRecv(Socket s, i32 answer)
{
	i64 err;

	if (answer == 0) return RecvResult::kDisconnect;
	else if (answer == -1)
	{

#ifdef _WIN32
		err = ConvertError();
		if (!err) {
			SockLen_t len = sizeof(err);
			getsockopt(s, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&err), &len);
		}
#else
		SockLen_t len = sizeof(err);
		getsockopt(s, SOL_SOCKET, SO_ERROR, &err, &len);
		if (!err) err = errno;
#endif
		switch (err)
		{
		case 0:
			break;
		case EAGAIN:
			return RecvResult::kAgain;
		default:
			return RecvResult::kDisconnect;
		}
	}
	return RecvResult::kOk;
}

RecvResult SocketTCP::SocketCalls::Recv(Socket s, DataBuffer* data, u64 bytesToRead, i32 flags)
{
	if (data == nullptr)
		return RecvResult::kOk;

	data->resize(bytesToRead);

	WIN(if (u_long t = 1; SOCKET_ERROR == ioctlsocket(s, FIONBIO, &t)) return RecvResult::kDisconnect;)

		i32 answer = recv(s, reinterpret_cast<char*>(data->data()), static_cast<i32>(bytesToRead), flags);
	RecvResult result = CheckRecv(s, answer);

	WIN(if (u_long t = 0; SOCKET_ERROR == ioctlsocket(s, FIONBIO, &t)) return RecvResult::kDisconnect;)

		return result;
}

void SocketTCP::SocketCalls::FdSet(Socket s, fd_set* set) {
	FD_SET(s, set);
}

bool SocketTCP::SocketCalls::FdIsSet(Socket s, fd_set* set) {
	return FD_ISSET(s, set);
}