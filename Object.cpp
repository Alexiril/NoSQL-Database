#include "Object.hpp"

void Database::Object::Clear(bool locked)
{
    if (not locked)
        accessMutex.lock();
    std::map<string, std::shared_ptr<Object>> childrenCopy = children;
    children.clear();
    for (auto& kid : childrenCopy)
    {
        kid.second->Clear(false);
        kid.second.reset();
    }
    childrenCopy.clear();
    if (not locked)
        accessMutex.unlock();
}

string Database::Object::Get(bool locked)
{
    sstream result;
    if (not locked)
        accessMutex.lock();
    result << "Objects: " << children.size() << std::endl;
    result << "[" << (children.size() > 0 ? "\n" : "");
    for (auto& [kid, object] : children)
        result << "   " << kid << "," << std::endl;
    result << "]" << std::endl;
    if (not locked)
        accessMutex.unlock();
    return result.str();
}

string Database::Object::Tree(u64 depth, bool locked)
{
    sstream result;
    if (not locked)
        accessMutex.lock();
    std::map <string, std::shared_ptr<Object>> childrenCopy = children;
    if (not locked)
        accessMutex.unlock();
    for (auto& [kid, object] : childrenCopy)
        result
        << string(depth, ' ')
        << kid
        << ":"
        << std::endl
        << object->Tree(depth + 1, false);
    return result.str();
}

string Database::Object::Export(string parent, bool locked)
{
    sstream result;
    if (not locked)
        accessMutex.lock();
    if (children.size() == 0)
    {
        if (not locked)
            accessMutex.unlock();
        return "";
    }
    result << "set ";
    if (parent != "database")
        result << parent << " ";
    std::map <string, std::shared_ptr<Object>> childrenCopy = children;
    if (not locked)
        accessMutex.unlock();
    for (auto& [kid, object] : children)
        result << kid << ",";
    result << std::endl;
    for (auto& [kid, object] : children)
        result << object->Export(std::format("{} {}", parent, kid), false);
    return result.str();
}

void Database::Object::Wait()
{
    if (anchors.load() == 0)
        return;
    std::unique_lock lock(waitingMutex);
    waiting.wait(lock, [this]()
        { return anchors.load() == 0; });
}

Database::RequestStateObject Database::Object::HandleRequest(string request)
{
    StringExtension::Trim(request);
    u64 spacePosition = request.find(' ');
    if (spacePosition != string::npos)
    {
        string command = request.substr(0, spacePosition);
        StringExtension::ToLower(command);
        if (not operations().contains(command))
            return RequestStateObject(
                std::format("Incorrect request '{}'. Read 'help' for the list of accepted requests.\n", command),
                RequestState::warning
            );
        string selector = request.substr(spacePosition + 1, UINT64_MAX);
        return HandleSelector(command, StringExtension::SplitStd(selector));
    }

    if (operations().contains(request))
        return HandleSelector(request, std::vector<string>());

    StringExtension::ToLower(request);
    if (request == "help" or request == "?")
    {
        sstream out;
        out << "Avaliable requests:" << std::endl;
        for (auto& [key, value] : operations())
            out << std::format("    {} - {}", key, get<1>(value)) << std::endl;
        if (operations().size() == 0)
            out << "    Actually, there's no requests. Sorry :)" << std::endl;
        return RequestStateObject(out.str(), RequestState::info);
    }

    return RequestStateObject(
        std::format("Incorrect request '{}'. Read 'help' for the list of accepted requests.\n", request),
        RequestState::warning
    );
}

bool Database::Object::ContainsCommand(string command)
{
    return operations().contains(command);
}

void Database::Object::SetThisPtr(std::weak_ptr<Object> this_ptr)
{
    this->this_ptr = this_ptr;
}

Database::RequestStateObject Database::Object::HandleSelector(string function, std::vector<string> selector)
{
    if (selector.size() <= 1)
    {
        anchors.fetch_add(1);
        accessMutex.lock();
        auto result = HandleComma(function, selector.size() > 0 ? StringExtension::Split(selector[0], ",") : std::vector<string>());
        accessMutex.unlock();
        anchors.fetch_add(-1);
        waiting.notify_one();
        return result;
    }
    if (not children.contains(selector.front()))
        return RequestStateObject(std::format("Object '{}' not found.", selector.front()), RequestState::error);
    anchors.fetch_add(1);
    string kid = selector.front();
    selector.erase(selector.begin());
    auto result = children[kid]->HandleSelector(function, selector);
    anchors.fetch_add(-1);
    waiting.notify_one();
    return result;
}

Database::RequestStateObject Database::Object::HandleComma(string function, const std::vector<string> commaObjects)
{
    sstream result;
    if (commaObjects.size() == 0)
        return RequestStateObject(get<0>(operations()[function])("").ColorizedMessage() + "\n", RequestState::none);
    for (auto& operand : commaObjects)
        if (not operand.empty() and operand != "database")
            result << get<0>(operations()[function])(operand).ColorizedMessage() << std::endl;
        else if (operand == "database")
            return RequestStateObject("You cannot name an object 'database'.", RequestState::error);
    return RequestStateObject(result.str(), RequestState::none);
}

Database::Object::Object(string name) : name(name)
{
}

Database::Object::~Object()
{
    Clear(false);
}

std::map<string, std::tuple<Database::Object::requestOperationType, string>> Database::Object::operations()
{

    return std::map<string, std::tuple<requestOperationType, string>>
    {
        std::make_pair("set", std::make_tuple([this](string value) { return Set(value); }, "set [selector] sets a value in the database.")),
            std::make_pair("get", std::make_tuple([this](string value) { return Get(value); }, "get[selector] shows the values(children objects) of the object.")),
            std::make_pair("tree", std::make_tuple([this](string value) { return Tree(value); }, "tree [selector] shows the entire object tree.")),
            std::make_pair("export", std::make_tuple([this](string value) { return Export(value); }, "export [selector] prints requests corresponding the data in the database.")),
            std::make_pair("check", std::make_tuple([this](string value) { return Check(value); }, "check [selector] checks if the object with the selector exists in the database now.")),
            std::make_pair("remove", std::make_tuple([this](string value) { return Remove(value); }, "remove [selector] deletes the object from the database.")),
            std::make_pair("clear", std::make_tuple([this](string value) { return Clear(value); }, "clear [selector] removes each child object from the selected object."))
    };
}

Database::RequestStateObject Database::Object::Set(string name)
{
    if (name == "")
        return RequestStateObject("Please specify the name of the created object.", RequestState::error);
    if (name.find(',') != string::npos)
        return RequestStateObject(std::format("Object cannot have a comma in the value: '{}'.", name), RequestState::error);
    if (children.contains(name))
        return RequestStateObject(std::format("Object '{}' is already in the '{}'.", name, this->name), RequestState::warning);
    auto kid = std::shared_ptr<Object>();
    kid.reset(new Object(name));
    kid->this_ptr = kid;
    kid->parent = this_ptr;
    children[name] = kid;
    return RequestStateObject(std::format("Object '{}' have been successfully set in the '{}'.", name, this->name), RequestState::ok);
}

Database::RequestStateObject Database::Object::Get(string name)
{
    if (name == "")
        return RequestStateObject(Get(true), RequestState::ok);
    if (not children.contains(name))
        return RequestStateObject(std::format("Object '{}' is not in the '{}'.", name, this->name), RequestState::error);
    return RequestStateObject(children[name]->Get(false), RequestState::ok);
}

Database::RequestStateObject Database::Object::Tree(string name)
{
    if (name == "")
        return RequestStateObject(Tree(0, true), RequestState::ok);
    if (not children.contains(name))
        return RequestStateObject(std::format("Object '{}' is not in the '{}'.", name, this->name), RequestState::error);
    return RequestStateObject(std::format("{}:\n{}", name, children[name]->Tree(1, false)), RequestState::ok);
}

Database::RequestStateObject Database::Object::Export(string name)
{
    if (name == "")
        return RequestStateObject(Export(this->name, true), RequestState::ok);
    if (not children.contains(name))
        return RequestStateObject(std::format("Object '{}' is not in the '{}'.", name, this->name), RequestState::error);
    return RequestStateObject(std::format("set {}\n{}", name, children[name]->Export(name, false)), RequestState::ok);
}

Database::RequestStateObject Database::Object::Check(string name)
{
    if (name == "")
        return RequestStateObject("Please specify the name of the created object.", RequestState::error);
    if (not children.contains(name))
        return RequestStateObject(std::format("Object '{}' is not in the '{}'.", name, this->name), RequestState::warning);
    return RequestStateObject(std::format("Object '{}' is in the '{}'.", name, this->name), RequestState::ok);
}

Database::RequestStateObject Database::Object::Remove(string name)
{
    if (name == "")
        return RequestStateObject("Please specify the name of the created object.", RequestState::error);
    if (not children.contains(name))
        return RequestStateObject(std::format("Object '{}' is not in the '{}'.", name, this->name), RequestState::error);
    auto& kid = children[name];
    kid->accessMutex.lock();
    kid->Wait();
    kid->Clear(true);
    kid->accessMutex.unlock();
    kid.reset();
    children.erase(name);
    return RequestStateObject(std::format("Object '{}' was successfully removed.", name), RequestState::ok);
}

Database::RequestStateObject Database::Object::Clear(string name)
{
    if (name == "")
    {
        anchors.fetch_add(-1);
        Wait();
        anchors.fetch_add(1);
        Clear(true);
        return RequestStateObject("Database was successfully cleared.", RequestState::ok);
    }
    if (not children.contains(name))
        return RequestStateObject(std::format("Object '{}' is not in the '{}'.", name, this->name), RequestState::error);
    auto& kid = children[name];
    kid->accessMutex.lock();
    kid->Wait();
    kid->Clear(true);
    kid->accessMutex.unlock();
    return RequestStateObject(std::format("Object '{}' was successfully cleared.", name), RequestState::ok);
}
