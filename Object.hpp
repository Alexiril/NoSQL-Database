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
        void Clear();
        string Get();
        string Tree(u64 depth);
        string ExportBranch(const string& parent);
        void Wait();

    protected:
        using requestOperationType = std::function<RequestStateObject(string value)>;

        std::map<string, std::tuple<requestOperationType, string>> operations();
        
        std::condition_variable_any waiting_;
        std::atomic<u64>            anchors_ = 0;
        rmutex			            access_mutex_;
        rmutex					    waiting_mutex_;

        string name_ = "";

        std::map<string, std::shared_ptr<Object>> children_{};
        std::weak_ptr<Object> parent_;
        std::weak_ptr<Object> this_ptr_;

        RequestStateObject HandleSelector(string& function, std::vector<string>& selector);
        RequestStateObject HandleComma(string& function, const std::vector<string>& commaObjects);

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

        RequestStateObject HandleRequest(string& request);
        
        bool ContainsCommand(const string& command);

        void SetThisPtr(std::weak_ptr<Object> this_ptr);
    };
}

#endif // !OBJECT_HPP
