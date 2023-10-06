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

        void ClearInternal();
        string GetInternal();
        string TreeInternal(u64 depth);
        string ExportInternal(const string& parent);
        void Wait();

    protected:
        
        std::condition_variable_any waiting_;
        std::atomic<u64>            anchors_ = 0;
        rmutex			            access_mutex_;
        rmutex					    waiting_mutex_;

        string name_ = "";

        std::map<string, std::shared_ptr<Object>> children_{};
        std::weak_ptr<Object> parent_;
        std::weak_ptr<Object> this_ptr_;

        RequestStateObject HandleSelector(const std::function<RequestStateObject(Object* object, string value)>& function, std::vector<string>& selector);
        RequestStateObject HandleComma(const std::function<RequestStateObject(Object* object, string value)>& function, const std::vector<string>& commaObjects);

    public:
        
        using requestOperationType = std::function<RequestStateObject(Object* object, string value)>;

        Object(string name);
        ~Object();

        RequestStateObject HandleRequest(string& request);
        
        bool ContainsCommand(const string& command);

        void SetThisPtr(std::weak_ptr<Object> this_ptr);

        RequestStateObject Set(string name);
        RequestStateObject Get(string name);
        RequestStateObject Tree(string name);
        RequestStateObject Export(string name);
        RequestStateObject Check(string name);
        RequestStateObject Remove(string name);
        RequestStateObject Clear(string name);
    };

    static const std::map<string, std::tuple<Object::requestOperationType, string>> kOperations {
        { "set", { &Object::Set, "set [selector] sets a value in the database." } },
        { "get", { &Object::Get, "get[selector] shows the values(children objects) of the object." } },
        { "tree", { &Object::Tree, "tree [selector] shows the entire object tree." } },
        { "export", { &Object::Export , "export [selector] prints requests corresponding the data in the database." } },
        { "check", { &Object::Check, "check [selector] checks if the object with the selector exists in the database now." } },
        { "remove", { &Object::Remove, "remove [selector] deletes the object from the database." } },
        { "clear", { &Object::Clear, "clear [selector] removes each child object from the selected object." } }
    };
}

#endif // !OBJECT_HPP
