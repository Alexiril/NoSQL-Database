#include "Application.hpp"

Application::Application()
{
    root_.reset(new Database::Object("database"));
    root_->SetThisPtr(root_);
}

Application::~Application()
{
    root_.reset();
}

void Application::Run()
{
    u32 ip_address_{ INADDR_ANY };
    u16 port_{ 1111 };
    u64 threadsAmount{ std::thread::hardware_concurrency() };

    string value;

    std::cout
        << "\033[92mApplication started.\033[0m"
        << std::endl
        << "  \033[92mPlease, set the listening IPv4 address (none for any): \033[0m";
    std::getline(std::cin, value);
    StringExtension::Trim(value);
    while (not value.empty())
    {
        try
        {
            inet_pton(AF_INET, value.c_str(), &ip_address_);
            break;
        }
        catch (const std::exception& exception)
        {
            std::cout
                << std::format("\033[91mSorry, it's not a correct IPv4 address: {}\033[0m", exception.what())
                << std::endl
                << "  \033[92mPlease, set the listening IPv4 address (none for any): \033[0m";
        }
        std::getline(std::cin, value);
    }
    std::cout << "  \033[92mPlease, set the listening port (none for standard): \033[0m";
    std::getline(std::cin, value);
    StringExtension::Trim(value);
    while (not value.empty())
    {
        try
        {
            port_ = std::stoi(value);
            break;
        }
        catch (const std::exception& exception)
        {
            std::cout
                << std::format("\033[91mSorry, it's not a correct port: {}\033[0m", exception.what())
                << std::endl
                << "  \033[92mPlease, set the listening port (none for standard): \033[0m";
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

    server_.reset(new SocketTCP::Server::Server(
        ip_address_,
        port_,
        { 1, 1, 1 },
        [this](SocketTCP::DataBuffer data, SocketTCP::Server::Server::Client& client)
        {
            string request((char*)data.data());
            sstream sendData;
            Database::RequestStateObject result{};
            if (request == "$dscn")
            {
                client.disconnect();
                return;
            }

            try
            {
                result = root_->HandleRequest(request);
            }
            catch (const std::exception& exception)
            {
                sendData
                    << std::format("\033[91mSome unexpected exception has happened:\n{}\n\033[0m", exception.what())
                    << "\033[93mPlease, contact the developer about this issue.\033[0m"
                    << std::endl;
                client.sendData(sendData.str());
                return;
            }

            sendData << result.ColorizedMessage();
            client.sendData(sendData.str());
        },
        [](SocketTCP::Server::Server::Client& client)
        {
            client.sendData("\033[93mWelcome! Database is active.\033[0m\n");
        },
        [](SocketTCP::Server::Server::Client& client) {},
        threadsAmount));

    serverThread_ = std::jthread([&]()
        {
            try {
                if (server_->start() == SocketTCP::Server::Server::Status::kUp)
                    server_->joinLoop();
                else
                {
                    std::cout
                        << "\n\033[91mServer start error! "
                        << server_->explainStatus()
                        << "\n\033[m..."
                        << std::endl;
                    exit(i32(server_->getStatus()));
                }
            }
            catch (std::exception& except) {
                std::cerr << except.what();
            }
            server_.reset(); });

    std::cout << "\033[92mDatabase server started.\033[0m" << std::endl;
    std::cout << "\033[96mSee 'help' for allowed commands.\033[0m" << std::endl;

    bool state_ = true;
    string input = "#";
    while (state_)
    {
        try
        {
            state_ = HandleConsole(input);
        }
        catch (const std::exception& exception)
        {
            std::cout
                << std::format("\033[91mSome unexpected exception has happened:\n{}\n\033[0m", exception.what())
                << "\033[93mPlease, contact the developer about this issue.\033[0m"
                << std::endl;
        }

        if (state_)
        {
            std::cout << "\033[95m>>\033[0m ";
            std::getline(std::cin, input);
        }
    }

    std::cout << "\033[93mHalting...\033[0m" << std::endl;

    server_->halt();
    serverThread_.join();
}

bool Application::HandleConsole(string& command)
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
        string helpCommand = "help";
        auto result = root_->HandleRequest(helpCommand);
        std::cout << result.ColorizedMessage() << std::endl;
        return true;
    }

    if (serverCommand == "status" or serverCommand == "state")
    {
        std::cout
            << "\033[96mStatus:"
            << std::endl
            << std::format("  state: {}", server_->getStatus() == SocketTCP::Server::Server::Status::kUp ? "\033[92mokay" : "\033[91mstopped")
            << std::endl
            << "\033[96m"
            << std::format("  address: {}:{}", SocketTCP::U32ToIpAddress(server_->getIpAddress()), server_->getPort())
            << std::endl
            << std::format("  threads: {}", server_->getThreadPool().getThreadCount())
            << std::endl
            << "\033[0m";
        return true;
    }

    if (serverCommand.find(' ') == string::npos and not root_->ContainsCommand(serverCommand))
    {
        std::cout << std::format("\033[91mUnknown command: '{}'\033[0m", command) << std::endl;
        return true;
    }
    else if (serverCommand.find(' ') != string::npos)
        serverCommand = command.substr(0, command.find(' '));

    if (root_->ContainsCommand(serverCommand))
    {
        std::cout << "\033[96m Requesting a database... It may take some time.\033[0m" << std::endl;
        Database::RequestStateObject result = root_->HandleRequest(command);
        std::cout << result.ColorizedMessage() << std::endl;
    }

    return true;
}