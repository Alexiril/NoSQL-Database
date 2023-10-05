#pragma once
#ifndef SHARED_HPP
#define SHARED_HPP

#include <vector>
#include <string>
#include <algorithm>
#include <condition_variable>
#include <cctype>
#include <cstring>
#include <locale>
#include <iostream>
#include <fstream>
#include <format>
#include <memory>
#include <map>
#include <sstream>
#include <queue>
#include <list>
#include <cstdint>
#include <thread>
#include <mutex>
#include <format>
#include <tuple>
#include <numeric>
#include <functional>

using string = std::string;
using mutex = std::mutex;
using rmutex = std::recursive_mutex;
using sstream = std::stringstream;

using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;

using i8 = int8_t;
using i16 = int16_t;
using i32 = int32_t;
using i64 = int64_t;

namespace StringExtension {

	inline void LeftTrim(string& s) {
		s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch)
			{ return !std::isspace(ch); }));
	}

	inline void RightTrim(string& s) {
		s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch)
			{ return !std::isspace(ch); })
			.base(),
			s.end());
	}

	inline void Trim(string& s) {
		RightTrim(s);
		LeftTrim(s);
	}

	inline void ToLower(string& s) {
		std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c)
			{ return std::tolower(c); });
	}

	inline const std::vector<string> Split(const string& s, const string& separator) {
		
		auto arguments = std::vector<string>();
		if (separator == "")
		{
			auto symbols = std::vector<char>(s.begin(), s.end());
			for (auto c : symbols)
				arguments.push_back(string{ c });
			return arguments;
		}
		u64 separatorLength = separator.length();
		u64 startIndex = 0;
		u64 separatorPosition = s.find(separator, startIndex);
		while (separatorPosition != string::npos)
		{
			arguments.push_back(s.substr(startIndex, separatorPosition - startIndex));
			startIndex = separatorPosition + separatorLength;
			separatorPosition = s.find(separator, startIndex);
		}
		arguments.push_back(s.substr(startIndex));
		return arguments;
	}

	inline std::vector<string> SplitStd(const string& s) {
		auto arguments = std::vector<string>();
		string word;
		sstream buffer(s);
		while (buffer >> word)
			arguments.push_back(word);
		return arguments;
	}
}

#endif // !SHARED_HPP
