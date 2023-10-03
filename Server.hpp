#pragma once
#ifndef SERVER_HPP
#define SERVER_HPP

#include "Shared.hpp"
#include "ThreadPool.hpp"
#include "Sockets.hpp"

namespace SocketTCP
{
    namespace Server
    {
        struct KeepAliveConfig
        {
            KaPropertyType idle = 120;
            KaPropertyType intvl = 3;
            KaPropertyType cnt = 5;
        };

        struct Server
        {
            struct Client;
            using HandlerFunctionType = std::function<void(DataBuffer, Client&)>;
            using ConnectionHandlerFunctionType = std::function<void(Client&)>;
            using ClientIterator = std::list<std::unique_ptr<Client>>::iterator;
            static constexpr auto default_data_handler_ = [](DataBuffer, Client&) {};
            static constexpr auto default_connection_handler_ = [](Client&) {};

            enum class Status : u8
            {
                kUp,
                kErrSocketInit,
                kErrSocketBind,
                kErrSocketKeepAlive,
                kErrSocketListening,
                kClose
            };

        private:
            Socket socket_;
            u16 port_;
            u32 ip_address_;
            Status status_ = Status::kClose;
            HandlerFunctionType handler = default_data_handler_;
            ConnectionHandlerFunctionType connect_handler_ = default_connection_handler_;
            ConnectionHandlerFunctionType disconnect_handler_ = default_connection_handler_;

            bool halts_ = false;
            ThreadPool thread;
            KeepAliveConfig ka_config;
            std::list<std::unique_ptr<Client>> clients_;
            mutex client_work_mutex_;
            u32 timeout_;

            bool enableKeepAlive(Socket socket_);
            void handlingAcceptLoop();
            void waitingDataLoop();

        public:
            Server(const u32 ip_address_, const u16 port_,
                KeepAliveConfig conf = {},
                HandlerFunctionType handler = default_data_handler_,
                ConnectionHandlerFunctionType connect_handler_ = default_connection_handler_,
                ConnectionHandlerFunctionType disconnect_handler_ = default_connection_handler_,
                u64 thread_count = std::thread::hardware_concurrency());

            ~Server();

            void setHandler(HandlerFunctionType handler);

            ThreadPool& getThreadPool() { return thread; }

            u16 getPort() const;
            u16 setPort(const u16 port);
            u32 getIpAddress() const;
            u32 setIpAddress(const u32 ip_address);
            Status getStatus() const { return status_; }

            Status start();
            void stop();
            void joinLoop();
            void halt();

            bool connectTo(u32 host, u16 port, ConnectionHandlerFunctionType connect_handler_);
            void sendData(const string& data);
            bool sendDataBy(u32 host, u16 port, const string& data);
            bool disconnectBy(u32 host, u16 port);
            void disconnectAll();

            string explainStatus();
        };

        struct Server::Client
        {
            friend struct Server;

            mutex access_mutex_;
            SocketAddr_in address_;
            Socket socket_;
            ClientSocketStatus status_ = ClientSocketStatus::kConnected;
            std::atomic_bool answered_ = false;
            std::atomic_bool removed_ = false;

        public:
            Client(Socket socket, SocketAddr_in address);
            ~Client();
            u32 getHost() const;
            u16 getPort() const;
            ClientSocketStatus getStatus() const { return status_; }
            ClientSocketStatus disconnect();

            DataBuffer loadData();
            RecvResult checkRecv(i32 answer);
            bool sendData(const string& data) const;
            SocketType getType() const { return SocketType::kServerSocket; }
        };

        string getHostStr(const Server::Client& client);
    }
}

#endif // !SERVER_HPP
