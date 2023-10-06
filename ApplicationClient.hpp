#pragma once
#ifndef APPLICATIONCLIENT_HPP
#define APPLICATIONCLIENT_HPP

#include "Shared.hpp"
#include "Sockets.hpp"
#include "Client.hpp"

#ifdef _WIN32
#include <io.h>
#define isatty _isatty
#define fileno _fileno
#endif // _WIN32

bool IsConsole();

void HandleClient(SocketTCP::Client::Client& client, bool console, SocketTCP::DataBuffer& data, std::istream& input);

#endif // !APPLICATIONCLIENT_HPP
