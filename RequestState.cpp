#include "RequestState.hpp"

Database::RequestStateObject::RequestStateObject()
{
	message = "";
	state = RequestState::none;
}

Database::RequestStateObject::RequestStateObject(string message, RequestState state) : message(message), state(state)
{
}

string Database::RequestStateObject::ColorizedMessage() const&
{
    sstream result;
	if (state == RequestState::ok)
		result << "\033[92m";
	if (state == RequestState::warning)
		result << "\033[93m";
	if (state == RequestState::error)
		result << "\033[91m";
	if (state == RequestState::info)
		result << "\033[96m";
	result << message;
	if (state != RequestState::none)
		result << " \033[0m";
	return result.str();
}
