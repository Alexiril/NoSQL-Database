#pragma once
#ifndef APPLICATION_HPP
#define APPLICATION_HPP

#include "Shared.hpp"
#include "Object.hpp"
#include "Server.hpp"

class ApplicationServer
{
public:
    ApplicationServer();
    ~ApplicationServer();

    void Run();

private:
    std::shared_ptr<Database::Object> root_;
    std::unique_ptr<SocketTCP::Server::Server> server_;
    std::jthread serverThread_;

    bool HandleConsole(string& request);
};

#endif // !APPLICATION_HPP
