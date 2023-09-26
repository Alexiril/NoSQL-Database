#pragma once
#ifndef CLIENTTCP_HPP
#define CLIENTTCP_HPP

#include "Sockets.hpp"
#include "ThreadPool.hpp"

namespace SocketTCP
{

    class ClientTCP : public ClientTCPBase
    {

        enum class ThreadManagementType : bool {
            single_thread = false,
            thread_pool = true
        };

        union Thread {
            std::jthread* thread;
            ThreadPool* thread_pool;
            Thread() : thread(nullptr) {}
            Thread(ThreadPool* thread_pool) : thread_pool(thread_pool) {}
            ~Thread() {}
        };

        SocketAddr_in _address;
        Socket _socket;

        mutex _handleMutex;
        std::function<void(DataBuffer)> _handler = [](DataBuffer) {};

        ThreadManagementType _threadManagementType;
        Thread _threads;

        SocketStatus _status = SocketStatus::disconnected;

        void handleSingleThread();
        void handleThreadPool();

    public:
        using handlerFunctionType = std::function<void(DataBuffer)>;
        ClientTCP() noexcept;
        ClientTCP(ThreadPool* thread_pool) noexcept;
        virtual ~ClientTCP() override;

        SocketStatus connectTo(u32 host, u16 port) noexcept;
        virtual SocketStatus disconnect() noexcept override;

        virtual u32 getHost() const override;
        virtual u16 getPort() const override;
        virtual SocketStatus getStatus() const override { return _status; }

        virtual DataBuffer loadData() override;
        DataBuffer loadDataSync();
        void setHandler(handlerFunctionType handler);
        void joinHandler();

        virtual bool sendData(const char* buffer, const u64 size) const override;
        virtual SocketType getType() const override { return SocketType::_socket; }
    };
}

#endif // !CLIENTTCP_HPP
