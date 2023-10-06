#pragma once
#ifndef CLIENT_HPP
#define CLIENT_HPP

#include "Sockets.hpp"

namespace SocketTCP
{
    namespace Client
    {
        class Client
        {
            SocketAddr_in address_;
            Socket socket_;

            mutex handle_mutex_;
            std::function<void(DataBuffer)> handler_;

            std::jthread thread;

            u32 timeout_;

            ClientSocketStatus status_ = ClientSocketStatus::kDisconnected;

            void handle();

        public:
            using HandlerFunctionType = std::function<void(DataBuffer)>;
            Client() noexcept;
            ~Client();

            ClientSocketStatus connectTo(u32 host, u16 port) noexcept;
            ClientSocketStatus disconnect() noexcept;

            u32 getHost() const;
            u16 getPort() const;
            ClientSocketStatus getStatus() const { return status_; }

            DataBuffer loadData();
            bool setHandler(HandlerFunctionType handler);
            void joinHandler();

            bool sendData(const string& data) const;
            SocketType getType() const { return SocketType::kClientSocket; }

            ISocketCalls* sockets = new SocketCalls();
        };
    }
}

#endif // !CLIENT_HPP
