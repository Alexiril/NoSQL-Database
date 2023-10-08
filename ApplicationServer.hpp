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

    void Run(std::istream& in);
    void RunConsole(std::istream& in);
    
    std::shared_ptr<Database::Object> root_;

protected:
    std::unique_ptr<SocketTCP::Server::Server> server_;
    std::jthread server_thread_;

    bool HandleConsole(string& request);
};

#endif // !APPLICATION_HPP
