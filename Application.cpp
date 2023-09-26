#include "Application.hpp"

Application::Application()
{
    _root.reset(new Database::Object("database"));
    _root->SetThisPtr(_root);
}

Application::~Application()
{
    _root.reset();
}

void Application::Run()
{
    u32 ipAddress{ INADDR_ANY };
    u16 port{ 1111 };
    u64 threadsAmount{ std::thread::hardware_concurrency() };

    string value;

    std::cout
        << "\033[92mApplication started.\033[0m"
        << std::endl
        << "  \033[92mPlease, set the std::listening IPv4 address (none for any): \033[0m";
    std::getline(std::cin, value);
    StringExtension::Trim(value);
    while (not value.empty())
    {
        try
        {
            inet_pton(AF_INET, value.c_str(), &ipAddress);
            break;
        }
        catch (const std::exception& exception)
        {
            std::cout
                << std::format("\033[91mSorry, it's not a correct IPv4 address: {}\033[0m", exception.what())
                << std::endl
                << "  \033[92mPlease, set the std::listening IPv4 address (none for any): \033[0m";
        }
        std::getline(std::cin, value);
    }
    std::cout << "  \033[92mPlease, set the std::listening port (none for standard): \033[0m";
    std::getline(std::cin, value);
    StringExtension::Trim(value);
    while (not value.empty())
    {
        try
        {
            port = std::stoi(value);
            break;
        }
        catch (const std::exception& exception)
        {
            std::cout
                << std::format("\033[91mSorry, it's not a correct port: {}\033[0m", exception.what())
                << std::endl
                << "  \033[92mPlease, set the std::listening port (none for standard): \033[0m";
        }
        std::getline(std::cin, value);
    }
    std::cout << "  \033[92mPlease, set the threads amount (none for automatic): \033[0m";
    std::getline(std::cin, value);
    StringExtension::Trim(value);
    while (not value.empty())
    {
        try
        {
            threadsAmount = std::stoi(value);
            break;
        }
        catch (const std::exception& exception)
        {
            std::cout
                << std::format("\033[91mSorry, it's not a correct number: {}\033[0m", exception.what())
                << std::endl
                << "  \033[92mPlease, set the threads amount (none for automatic): \033[0m";
        }
        std::getline(std::cin, value);
    }

    _server.reset(new SocketTCP::TcpServer(
        ipAddress,
        port,
        { 1, 1, 1 },
        [this](SocketTCP::DataBuffer data, SocketTCP::TcpServer::Client& client)
        {
            string request((char*)data.data());
            sstream sendData;
            Database::RequestStateObject result{};

            try
            {
                result = _root->HandleRequest(request);
            }
            catch (const std::exception& exception)
            {
                sendData
                    << std::format("\033[91mSome unexpected exception has happened:\n{}\n\033[0m", exception.what())
                    << "\033[93mPlease, contact the developer about this issue.\033[0m"
                    << std::endl;
                client.sendData(sendData.str().c_str(), sendData.str().size());
                return;
            }

            sendData << result.ColorizedMessage();
            client.sendData(sendData.str().c_str(), sendData.str().size());
        },
        [](SocketTCP::TcpServer::Client& client)
        {
            client.sendData("\033[93mWelcome! Database is active.\033[0m\n", 39);
        },
        [](SocketTCP::TcpServer::Client& client) {},
        threadsAmount));

    _serverThread = std::jthread([&]()
        {
            try {
                if (_server->start() == SocketTCP::TcpServer::status::up)
                    _server->joinLoop();
                else
                {
                    std::cout
                        << "\n\033[91mServer start error! "
                        << _server->explainStatus()
                        << "\n\033[m..."
                        << std::endl;
                    exit(i32(_server->getStatus()));
                }
            }
            catch (std::exception& except) {
                std::cerr << except.what();
            }
            _server.reset(); });

    _serverThread.detach();

    RunConsole();
}

bool Application::HandleConsole(string command)
{
    StringExtension::Trim(command);
    string serverCommand = command;
    StringExtension::ToLower(serverCommand);

    if (command.empty() or command.size() == 0 or command[0] == '#')
        return true;

    if (serverCommand == "exit" or serverCommand == "quit")
    {
        std::cout << "\033[92mSee you next time.\033[0m" << std::endl;
        return false;
    }

    if (serverCommand == "help" or serverCommand == "?")
    {
        string commands[] = {
            "exit    | quit    - stops and exits the program",
            "help    | ?       - shows this message",
            "status  | state   - shows the current server state"};
        std::cout << "\033[96mCommands:" << std::endl;
        for (auto& command : commands)
            std::cout << std::format("    {}", command) << std::endl;
        std::cout << "\033[0m" << std::endl;
        auto result = _root->HandleRequest("help");
        std::cout << result.ColorizedMessage() << std::endl;
        return true;
    }

    if (serverCommand == "status" or serverCommand == "state")
    {
        std::cout
            << "\033[96mStatus:"
            << std::endl
            << std::format("  state: {}", _server->getStatus() == SocketTCP::TcpServer::status::up ? "\033[92mokay" : "\033[91mstopped")
            << std::endl
            << "\033[96m"
            << std::format("  address: {}:{}", SocketTCP::U32ToIpAddress(_server->getIpAddress()), _server->getPort())
            << std::endl
            << std::format("  threads: {}", _server->getThreadPool().getThreadCount())
            << std::endl
            << "\033[0m";
        return true;
    }

    if (serverCommand.find(' ') == string::npos and not _root->ContainsCommand(serverCommand))
    {
        std::cout << std::format("\033[91mUnknown command: '{}'\033[0m", command) << std::endl;
        return true;
    }
    else if (serverCommand.find(' ') != string::npos)
        serverCommand = command.substr(0, command.find(' '));

    if (_root->ContainsCommand(serverCommand))
    {
        std::cout << "\033[96m Requesting a database... It may take some time.\033[0m" << std::endl;
        Database::RequestStateObject result = _root->HandleRequest(command);
        std::cout << result.ColorizedMessage() << std::endl;
    }

    return true;
}

void Application::RunConsole()
{
    std::cout << "\033[92mDatabase server started.\033[0m" << std::endl;
    std::cout << "\033[96mSee 'help' for allowed commands.\033[0m" << std::endl;

    bool state = true;
    string input = "#";
    while (state)
    {
        try
        {
            state = HandleConsole(input);
        }
        catch (const std::exception& exception)
        {
            std::cout
                << std::format("\033[91mSome unexpected exception has happened:\n{}\n\033[0m", exception.what())
                << "\033[93mPlease, contact the developer about this issue.\033[0m"
                << std::endl;
        }

        if (state)
        {
            std::cout << "\033[95m>>\033[0m ";
            std::getline(std::cin, input);
        }
    }

    _server->halt();
    _serverThread.request_stop();
    exit(0);
}
