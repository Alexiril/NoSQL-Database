#include "Object.hpp"

void Database::Object::ClearInternal()
{
	access_mutex_.lock();
	std::map<string, std::shared_ptr<Object>> childrenCopy = children_;
	children_.clear();
	for (auto& kid : childrenCopy)
	{
		kid.second->ClearInternal();
		kid.second.reset();
	}
	childrenCopy.clear();
	access_mutex_.unlock();
}

string Database::Object::GetInternal()
{
	sstream result;
	access_mutex_.lock();
	result << "Objects: " << children_.size() << std::endl;
	result << "[" << (children_.size() > 0 ? "\n" : "");
	for (auto& [kid, object] : children_)
		result << "   " << kid << "," << std::endl;
	result << "]" << std::endl;
	access_mutex_.unlock();
	return result.str();
}

string Database::Object::TreeInternal(u64 depth)
{
	sstream result;
	access_mutex_.lock();
	std::map <string, std::shared_ptr<Object>> childrenCopy = children_;
	access_mutex_.unlock();
	for (auto& [kid, object] : childrenCopy)
		result
		<< string(depth, ' ')
		<< kid
		<< ":"
		<< std::endl
		<< object->TreeInternal(depth + 1);
	return result.str();
}

string Database::Object::ExportInternal(const string& parent)
{
	sstream result;
	access_mutex_.lock();
	if (children_.size() == 0)
	{
		access_mutex_.unlock();
		return "";
	}
	result << "set ";
	if (parent != "database")
		result << parent << " ";
	std::map <string, std::shared_ptr<Object>> childrenCopy = children_;
	access_mutex_.unlock();
	for (auto& [kid, object] : children_)
		result << kid << ",";
	result << std::endl;
	for (auto& [kid, object] : children_)
		result << object->ExportInternal(std::format("{} {}", parent, kid));
	return result.str();
}

void Database::Object::Wait()
{
	if (anchors_.load() == 0)
		return;
	std::unique_lock lock(waiting_mutex_);
	waiting_.wait(lock, [this]()
		{ return anchors_.load() == 0; });
}

Database::RequestStateObject Database::Object::HandleRequest(string& request)
{
	StringExtension::Trim(request);
	u64 spacePosition = request.find(' ');
	if (spacePosition != string::npos)
	{
		string command = request.substr(0, spacePosition);
		StringExtension::ToLower(command);
		if (not kOperations.contains(command))
			return RequestStateObject(
				std::format("Incorrect request '{}'. Read 'help' for the list of accepted requests.\n", command),
				RequestState::kWarning
			);
		std::vector<string> selector = StringExtension::SplitStd(request.substr(spacePosition + 1, UINT64_MAX));
		auto timeStart = std::chrono::high_resolution_clock::now();
		auto result = HandleSelector(get<0>(kOperations.at(command)), selector);
		auto timeFinish = std::chrono::high_resolution_clock::now();
		std::chrono::duration<double, std::milli> milleseconds = timeFinish - timeStart;
		result.message_ += std::format("\n\033[96mRequest took {} seconds.\033[0m\n", milleseconds.count() / 1000);
		return result;
	}

	if (kOperations.contains(request))
	{
		auto timeStart = std::chrono::high_resolution_clock::now();
		auto empty = std::vector<string>();
		auto result = HandleSelector(get<0>(kOperations.at(request)), empty);
		auto timeFinish = std::chrono::high_resolution_clock::now();
		std::chrono::duration<double, std::milli> milleseconds = timeFinish - timeStart;
		result.message_ += std::format("\n\033[96mRequest took {} seconds.\033[0m\n", milleseconds.count() / 1000);
		return result;
	}

	StringExtension::ToLower(request);
	if (request == "help" or request == "?")
	{
		sstream out;
		out << "Avaliable requests:" << std::endl;
		for (auto& [key, value] : kOperations)
			out << std::format("    {} - {}", key, get<1>(value)) << std::endl;
		if (kOperations.size() == 0)
			out << "    Actually, there's no requests. Sorry :)" << std::endl;
		return RequestStateObject(out.str(), RequestState::kInfo);
	}

	return RequestStateObject(
		std::format("Incorrect request '{}'. Read 'help' for the list of accepted requests.\n", request),
		RequestState::kWarning
	);
}

bool Database::Object::ContainsCommand(const string& command)
{
	return kOperations.contains(command);
}

void Database::Object::SetThisPtr(std::weak_ptr<Object> this_ptr)
{
	this_ptr_ = this_ptr;
}

Database::RequestStateObject Database::Object::HandleSelector(const requestOperationType& function, std::vector<string>& selector)
{
	if (selector.size() <= 1)
	{
		anchors_.fetch_add(1);
		access_mutex_.lock();
		auto result = HandleComma(function, selector.size() > 0 ? StringExtension::Split(selector[0], ",") : std::vector<string>());
		access_mutex_.unlock();
		anchors_.fetch_add(-1);
		waiting_.notify_one();
		return result;
	}
	if (not children_.contains(selector.front()))
		return RequestStateObject(std::format("Object '{}' not found.", selector.front()), RequestState::kError);
	anchors_.fetch_add(1);
	string kid = selector.front();
	selector.erase(selector.begin());
	auto result = children_[kid]->HandleSelector(function, selector);
	anchors_.fetch_add(-1);
	waiting_.notify_one();
	return result;
}

Database::RequestStateObject Database::Object::HandleComma(const requestOperationType& function, const std::vector<string>& commaObjects)
{
	sstream result;
	if (commaObjects.size() == 0)
		return RequestStateObject(function(this, "").ColorizedMessage() + "\n", RequestState::kNone);
	for (auto& operand : commaObjects)
		if (not operand.empty() and operand != "database")
			result << function(this, operand).ColorizedMessage() << std::endl;
		else if (operand == "database")
			return RequestStateObject("You cannot name an object 'database'.", RequestState::kError);
	return RequestStateObject(result.str(), RequestState::kNone);
}

Database::Object::Object(string name) : name_(name)
{
}

Database::Object::~Object()
{
	ClearInternal();
}

Database::RequestStateObject Database::Object::Set(string name)
{
	if (name == "")
		return RequestStateObject("Please specify the name of the created object.", RequestState::kError);
	if (name.find(',') != string::npos)
		return RequestStateObject(std::format("Object cannot have a comma in the value: '{}'.", name), RequestState::kError);
	if (children_.contains(name))
		return RequestStateObject(std::format("Object '{}' is already in the '{}'.", name, name_), RequestState::kWarning);
	auto kid = std::shared_ptr<Object>();
	kid.reset(new Object(name));
	kid->this_ptr_ = kid;
	kid->parent_ = this_ptr_;
	children_[name] = kid;
	return RequestStateObject(std::format("Object '{}' have been successfully set in the '{}'.", name, name_), RequestState::kOk);
}

Database::RequestStateObject Database::Object::Get(string name)
{
	if (name == "")
		return RequestStateObject(GetInternal(), RequestState::kOk);
	if (not children_.contains(name))
		return RequestStateObject(std::format("Object '{}' is not in the '{}'.", name, name_), RequestState::kError);
	return RequestStateObject(children_[name]->GetInternal(), RequestState::kOk);
}

Database::RequestStateObject Database::Object::Tree(string name)
{
	if (name == "")
		return RequestStateObject(TreeInternal(0), RequestState::kOk);
	if (not children_.contains(name))
		return RequestStateObject(std::format("Object '{}' is not in the '{}'.", name, name_), RequestState::kError);
	return RequestStateObject(std::format("{}:\n{}", name, children_[name]->TreeInternal(1)), RequestState::kOk);
}

Database::RequestStateObject Database::Object::Export(string name)
{
	if (name == "")
		return RequestStateObject(ExportInternal(""), RequestState::kOk);
	if (not children_.contains(name))
		return RequestStateObject(std::format("Object '{}' is not in the '{}'.", name, name_), RequestState::kError);
	return RequestStateObject(std::format("set {}\n{}", name, children_[name]->ExportInternal(name)), RequestState::kOk);
}

Database::RequestStateObject Database::Object::Check(string name)
{
	if (name == "")
		return RequestStateObject("Please specify the name of the created object.", RequestState::kError);
	if (not children_.contains(name))
		return RequestStateObject(std::format("Object '{}' is not in the '{}'.", name, name_), RequestState::kWarning);
	return RequestStateObject(std::format("Object '{}' is in the '{}'.", name, name_), RequestState::kOk);
}

Database::RequestStateObject Database::Object::Remove(string name)
{
	if (name == "")
		return RequestStateObject("Please specify the name of the created object.", RequestState::kError);
	if (not children_.contains(name))
		return RequestStateObject(std::format("Object '{}' is not in the '{}'.", name, name_), RequestState::kError);
	auto& kid = children_[name];
	kid->access_mutex_.lock();
	kid->Wait();
	kid->ClearInternal();
	kid->access_mutex_.unlock();
	kid.reset();
	children_.erase(name);
	return RequestStateObject(std::format("Object '{}' was successfully removed.", name), RequestState::kOk);
}

Database::RequestStateObject Database::Object::Clear(string name)
{
	if (name == "")
	{
		anchors_.fetch_add(-1);
		Wait();
		anchors_.fetch_add(1);
		ClearInternal();
		return RequestStateObject("Database was successfully cleared.", RequestState::kOk);
	}
	if (not children_.contains(name))
		return RequestStateObject(std::format("Object '{}' is not in the '{}'.", name, name_), RequestState::kError);
	auto& kid = children_[name];
	kid->access_mutex_.lock();
	kid->Wait();
	kid->ClearInternal();
	kid->access_mutex_.unlock();
	return RequestStateObject(std::format("Object '{}' was successfully cleared.", name), RequestState::kOk);
}
