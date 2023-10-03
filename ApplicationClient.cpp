#include "Shared.hpp"
#include "Sockets.hpp"
#include "Client.hpp"

#ifdef _WIN32
#include <io.h>
#define isatty _isatty
#define fileno _fileno
#endif // _WIN32

i32 main(i32 argc, char **argv)
{
    bool console = isatty(fileno(stdin));
    using namespace std::chrono_literals;
    i32 serverIpAddress{0};
    inet_pton(AF_INET, "127.0.0.1", &serverIpAddress);
    u16 serverPort{1111};
    string ip_address, port;
    std::cout << "\033[92mDatabase client :)\033[0m" << std::endl;
    if (console)
        std::cout << "\033[95mSet IPv4 address (nothing for localhost): \033[0m";
    std::getline(std::cin, ip_address);
    StringExtension::Trim(ip_address);
    while (not ip_address.empty())
    {
        try
        {
            if (inet_pton(AF_INET, ip_address.c_str(), &serverIpAddress) != 1)
                throw std::runtime_error("");
            break;
        }
        catch (...)
        {
            std::cout
                << std::format("\033[91mSorry, it's not a correct IPv4 address\033[0m")
                << std::endl;
            if (console)
                std::cout << "\033[95mSet IPv4 address (nothing for localhost): \033[0m";
        }
        std::getline(std::cin, ip_address);
    }
    if (console)
        std::cout << "\033[95mSet server port (nothing for default): \033[0m";
    std::getline(std::cin, port);
    StringExtension::Trim(port);
    while (not port.empty())
    {
        try
        {
            serverPort = std::stoi(port);
            break;
        }
        catch (const std::exception &exception)
        {
            std::cout
                << std::format("\033[91mSorry, it's not a correct port: {}\033[0m", exception.what())
                << std::endl;
            if (console)
                std::cout << "\033[95mSet server port (nothing for default): \033[0m";
        }
        std::getline(std::cin, port);
    }
    auto client = SocketTCP::Client::Client();
    if (client.connectTo(serverIpAddress, serverPort) == SocketTCP::ClientSocketStatus::kConnected)
    {
        std::cout << "\033[92mConnected to the server successfully.\033[0m" << std::endl;

        client.setHandler([&client, console](SocketTCP::DataBuffer data)
                          {
                try
                {
                    if (data.size() > 0)
                    {
                        data.push_back('\0');
                        std::cout << reinterpret_cast<char*>(data.data());
                        if (console)
                            std::cout << std::endl;
                    }
                    string request;
                    if (console)
                        std::cout << "\033[95m>>\033[0m ";
                    std::getline(std::cin, request);
                    StringExtension::Trim(request);
                    if (request == "exit")
                    {
                        std::cout << "\033[92mSee you next time.\033[0m" << std::endl;
                        client.sendData("$dscn");
                        exit(0);
                    }
                    client.sendData(request);
                }
                catch (const std::exception& exception)
                {
                    std::cout << "\033[91mRuntime error:" << exception.what() << "\033[0m" << std::endl;
                } });
        client.joinHandler();
    }
    else
    {
        std::cout << "\033[91mClient couldn't connect to the server.\033[0m" << std::endl;
        exit(1);
    }
}
