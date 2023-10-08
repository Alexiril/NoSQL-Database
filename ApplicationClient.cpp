#include "ApplicationClient.hpp"

bool IsConsole()
{
	return isatty(fileno(stdin));
}

void HandleClient(SocketTCP::Client::Client& client, bool console, SocketTCP::DataBuffer& data, std::istream& input) {
	try
	{
		string request;

		data.push_back(0);

		if (console)
			std::cout << reinterpret_cast<char*>(data.data())
			<< (data[0] != 0 ? "\n" : "")
			<< "\033[95m>>\033[0m ";

		std::getline(input, request);
		StringExtension::Trim(request);

		if (request == "exit" or request == "quit") request = "$dscn";
		
		client.sendData(request);

		if (request == "$dscn")
			client.disconnect();
	}
	catch (const std::exception& exception)
	{
		std::cout << "\033[91mRuntime error:" << exception.what() << "\033[0m" << std::endl;
	}
}