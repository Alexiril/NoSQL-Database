#pragma once
#ifndef APPLICATION_HPP
#define APPLICATION_HPP

#include "Shared.hpp"
#include "Object.hpp"
#include "ServerTCP.hpp"

class Application
{
public:
    Application();
    ~Application();

    void Run();
    void RunConsole();

private:
    std::shared_ptr<Database::Object> _root;
    std::unique_ptr<SocketTCP::TcpServer> _server;
    std::jthread _serverThread;

    bool HandleConsole(string request);
};

#endif // !APPLICATION_HPP
