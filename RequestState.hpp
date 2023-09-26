#pragma once
#ifndef REQUESTSTATE_HPP
#define REQUESTSTATE_HPP

#include "Shared.hpp"

namespace Database
{
	enum class RequestState
	{
		none,
		ok,
		warning,
		error,
		info
	};

	class RequestStateObject
	{
	public:
		RequestState state = RequestState::none;
		string message = "";

		RequestStateObject();
		RequestStateObject(string message, RequestState state);

		string ColorizedMessage() const&;
	};
}

#endif // !REQUESTSTATE_HPP


