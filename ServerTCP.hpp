#pragma once
#ifndef SERVERTCP_HPP
#define SERVERTCP_HPP

#include "ThreadPool.hpp"
#include "Sockets.hpp"

namespace SocketTCP
{

    struct KeepAliveConfig
    {
        ka_prop_t ka_idle = 120;
        ka_prop_t ka_intvl = 3;
        ka_prop_t ka_cnt = 5;
    };

    struct TcpServer
    {
        struct Client;
        class ClientList;
        using handlerFunctionType = std::function<void(DataBuffer, Client&)>;
        using connectionHandlerFunctionType = std::function<void(Client &)>;
        static constexpr auto defaultDataHandler = [](DataBuffer, Client &) {};
        static constexpr auto defaultConnectionHandler = [](Client &) {};

        enum class status : u8
        {
            up = 0,
            err_socket_init = 1,
            err_socket_bind = 2,
            err_socket_keep_alive = 3,
            err_socket_listening = 4,
            close = 5
        };

    private:
        Socket serv_socket;
        u16 port;
        u32 ipAddress;
        status _status = status::close;
        handlerFunctionType handler = defaultDataHandler;
        connectionHandlerFunctionType connect_hndl = defaultConnectionHandler;
        connectionHandlerFunctionType disconnect_hndl = defaultConnectionHandler;

        bool halts = false;
        ThreadPool _threads;
        typedef std::list<std::unique_ptr<Client>>::iterator ClientIterator;
        KeepAliveConfig ka_conf;
        std::list<std::unique_ptr<Client>> clients;
        mutex clientMutex;

        bool enableKeepAlive(Socket socket);
        void handlingAcceptLoop();
        void waitingDataLoop();

    public:
        TcpServer(const u32 ipAddress, const u16 port,
                  KeepAliveConfig ka_conf = {},
                  handlerFunctionType handler = defaultDataHandler,
                  connectionHandlerFunctionType connect_hndl = defaultConnectionHandler,
                  connectionHandlerFunctionType disconnect_hndl = defaultConnectionHandler,
                  u64 threadCount = std::thread::hardware_concurrency());

        ~TcpServer();

        void setHandler(handlerFunctionType handler);

        ThreadPool &getThreadPool() { return _threads; }

        u16 getPort() const;
        u16 setPort(const u16 port);
        u32 getIpAddress() const;
        u32 setIpAddress(const u32 ipAddress);
        status getStatus() const { return _status; }

        status start();
        void stop();
        void joinLoop();
        void halt();

        bool connectTo(u32 host, u16 port, connectionHandlerFunctionType connect_hndl);
        void sendData(const char *buffer, const u64 size);
        bool sendDataBy(u32 host, u16 port, const char *buffer, const u64 size);
        bool disconnectBy(u32 host, u16 port);
        void disconnectAll();

        string explainStatus();
    };

    struct TcpServer::Client : public ClientTCPBase
    {
        friend struct TcpServer;

        mutex accessMutex;
        SocketAddr_in _address;
        Socket socket;
        SocketStatus _status = SocketStatus::connected;
        bool _answered = false;
        bool _removed = false;

    public:
        Client(Socket socket, SocketAddr_in _address);
        virtual ~Client() override;
        virtual u32 getHost() const override;
        virtual u16 getPort() const override;
        virtual SocketStatus getStatus() const override { return _status; }
        virtual SocketStatus disconnect() override;

        virtual DataBuffer loadData() override;
        virtual bool sendData(const char *buffer, const u64 size) const override;
        virtual SocketType getType() const override { return SocketType::server_socket; }
    };

    string getHostStr(const TcpServer::Client &client);

}

#endif // !SERVERTCP_HPP
