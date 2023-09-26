#pragma once
#ifndef OBJECT_HPP
#define OBJECT_HPP

#include "Shared.hpp"
#include "RequestState.hpp"

namespace Database
{
    class Object
    {

    private:
        void Clear(bool locked);
        string Get(bool locked);
        string Tree(u64 depth, bool locked);
        string Export(string parent, bool locked);
        void Wait();

    protected:
        using requestOperationType = std::function<RequestStateObject(string value)>;

        std::map<string, std::tuple<requestOperationType, string>> operations();

        std::atomic<u64>        anchors = 0;
        mutex			        accessMutex;
        mutex					waitingMutex;
        std::condition_variable waiting;

        string name = "";

        std::map<string, std::shared_ptr<Object>> children{};
        std::weak_ptr<Object> parent;
        std::weak_ptr<Object> this_ptr;

        RequestStateObject HandleSelector(string function, std::vector<string> selector);
        RequestStateObject HandleComma(string function, const std::vector<string> commaObjects);

        RequestStateObject Set(string name);
        RequestStateObject Get(string name);
        RequestStateObject Tree(string name);
        RequestStateObject Export(string name);
        RequestStateObject Check(string name);
        RequestStateObject Remove(string name);
        RequestStateObject Clear(string name);

    public:
        Object(string name);
        ~Object();

        RequestStateObject HandleRequest(string request);
        
        bool ContainsCommand(string command);

        void SetThisPtr(std::weak_ptr<Object> this_ptr);
    };
}

#endif // !OBJECT_HPP
