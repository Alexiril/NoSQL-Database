#include "ApplicationClient.hpp"

i32 main(i32 argc, char** argv)
{
	bool console = IsConsole();
	using namespace std::chrono_literals;
	i64 server_ip_address{ 0 };
	i64 server_port{ 1111 };

	if (console) std::cout << "\033[92mDatabase client :)\033[0m" << std::endl;

	GetValue(std::cin, "server IPv4 address", console, 0x0100007F, &SocketTCP::GetIPAddress, server_ip_address);
	GetValue(std::cin, "server port", console, 1111, &SocketTCP::GetPort, server_port);

	auto client = SocketTCP::Client::Client();
	if (client.connectTo(static_cast<i32>(server_ip_address), static_cast<u16>(server_port)) == SocketTCP::ClientSocketStatus::kConnected &&
		client.setHandler([&client, console](SocketTCP::DataBuffer data) { HandleClient(client, console, data, std::cin); }))
	{
		std::cout << "\033[92mConnected to the server successfully.\033[0m" << std::endl;
		client.joinHandler();
	}
	else std::cout << "\033[91mClient couldn't connect to the server.\033[0m" << std::endl;

	std::cout
		<< "\033[93mConnection halted.\033[0m" << std::endl
		<< "\033[92mSee you next time.\033[0m" << std::endl;
}