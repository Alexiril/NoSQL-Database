#pragma once
#ifndef REQUESTSTATE_HPP
#define REQUESTSTATE_HPP

#include "Shared.hpp"

namespace Database
{
	enum class RequestState
	{
		kNone,
		kOk,
		kWarning,
		kError,
		kInfo
	};

	class RequestStateObject
	{
	public:
		RequestState state_ = RequestState::kNone;
		string message_ = "";


		RequestStateObject()
		{
			message_ = "";
			state_ = RequestState::kNone;
		}

		RequestStateObject(string message, RequestState state) : message_(message), state_(state)
		{
		}

		string ColorizedMessage() const &
		{
			sstream result;
			if (state_ == RequestState::kOk)
				result << "\033[92m";
			if (state_ == RequestState::kWarning)
				result << "\033[93m";
			if (state_ == RequestState::kError)
				result << "\033[91m";
			if (state_ == RequestState::kInfo)
				result << "\033[96m";
			result << message_;
			if (state_ != RequestState::kNone)
				result << "\033[0m";
			return result.str();
		}
	};
}

#endif // !REQUESTSTATE_HPP


