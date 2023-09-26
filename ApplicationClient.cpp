#include "Shared.hpp"
#include "Sockets.hpp"
#include "ClientTCP.hpp"

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
    string ipAddress, port;
    std::cout << "\033[92mDatabase client :)\033[0m" << std::endl;
    if (console)
        std::cout << "\033[95mSet IPv4 address (nothing for localhost): \033[0m";
    std::getline(std::cin, ipAddress);
    StringExtension::Trim(ipAddress);
    while (not ipAddress.empty())
    {
        try
        {
            if (inet_pton(AF_INET, ipAddress.c_str(), &serverIpAddress) != 1)
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
        std::getline(std::cin, ipAddress);
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
    bool argvHandled = false, exportRequestSent = false;
    auto client = SocketTCP::ClientTCP();
    if (client.connectTo(serverIpAddress, serverPort) == SocketTCP::SocketStatus::connected)
    {
        std::cout << "\033[92mConnected to the server successfully.\033[0m" << std::endl;

        client.setHandler([&client, argc, argv, &argvHandled, &exportRequestSent, console](SocketTCP::DataBuffer data)
                          {
                try
                {
                    if ((not argvHandled and argc > 1) or exportRequestSent)
                    {
                        if (exportRequestSent)
                        {
                            string result = (char*)data.data();
                            result.erase(0, 5);
                            result.erase(result.size() - 4, 4);
                            StringExtension::Trim(result);
                            std::cout << SocketTCP::U32ToIpAddress(client.getHost()) << std::endl;
                            std::cout << ntohs(client.getPort()) << std::endl;
                            std::cout << result;
                            std::cout << "exit" << std::endl;
                            exit(0);
                        }
                        argvHandled = true;
                        if (argc > 4)
                        {
                            std::cout << std::format("\033[91mToo many command line arguments to handle ({}).\033[0m", argc) << std::endl;
                            exit(0);
                        }
                        if ((string)argv[1] == "export")
                        {
                            exportRequestSent = true;
                            client.sendData("export", 7);
                            return;
                        }
                        std::cout << std::format("\033[91mNot correct command line argument '{}'.\033[0m", argv[1]) << std::endl;
                        exit(0);
                    }
                    if (data.size() > 0)
                    {
                        data.push_back('\0');
                        std::cout << (string)(char*)data.data();
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
                        exit(0);
                    }
                    client.sendData(request.c_str(), request.size() + 1);
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
    return 0;
}
