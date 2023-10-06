#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include "../Shared.hpp"

using ::testing::ElementsAre;
using ::testing::ElementsAreArray;

namespace Shared {
	TEST(TypesSize, Predefined) {
		ASSERT_EQ(sizeof(u8), 1);
		ASSERT_EQ(sizeof(i8), 1);
		ASSERT_EQ(sizeof(u16), 2);
		ASSERT_EQ(sizeof(i16), 2);
		ASSERT_EQ(sizeof(u32), 4);
		ASSERT_EQ(sizeof(i32), 4);
		ASSERT_EQ(sizeof(u64), 8);
		ASSERT_EQ(sizeof(i64), 8);
	}

	TEST(GetNumericValue, StandardUsing) {
		i64 test_value;
		ASSERT_EQ(GetNumericValue("abc", test_value, 0, 10), false);
		ASSERT_EQ(GetNumericValue("-1", test_value, 0, 10), false);
		ASSERT_EQ(GetNumericValue("-1", test_value, -5, 0), true);
		ASSERT_EQ(test_value, -1);
		ASSERT_EQ(GetNumericValue("4095", test_value, 1000, 0x1000), true);
		ASSERT_EQ(test_value, 0xFFF);
	}
}

namespace StringExtension {
	TEST(LeftTrim, EmptyString) {
		string value = "";
		StringExtension::LeftTrim(value);
		ASSERT_EQ(value, "");
	}

	TEST(LeftTrim, StandardUsing) {
		std::map<string, string> variants = {
			{"abc", "abc"},
			{" abc", "abc"},
			{"\n abc \n", "abc \n"}
		};

		for (const auto& [key, value] : variants)
		{
			string value = key;
			StringExtension::LeftTrim(value);
			ASSERT_EQ(value, value);
		}
	}

	TEST(LeftTrim, SpecialSymbols) {
		std::map<string, string> variants = {
			{"\n .a", ".a"},
			{"\t\v\f  \rabc;\t", "abc;\t"},
			{"\0 abc \n", "abc \n"}
		};

		for (const auto& [key, value] : variants)
		{
			string value = key;
			StringExtension::LeftTrim(value);
			ASSERT_EQ(value, value);
		}
	}

	TEST(RightTrim, EmptyString) {
		string value = "";
		StringExtension::RightTrim(value);
		ASSERT_EQ(value, "");
	}

	TEST(RightTrim, StandardUsing) {
		std::map<string, string> variants = {
			{"abc", "abc"},
			{" abc ", " abc"},
			{"\n abc \n", "\n abc"}
		};

		for (const auto& [key, value] : variants)
		{
			string value = key;
			StringExtension::RightTrim(value);
			ASSERT_EQ(value, value);
		}
	}

	TEST(RightTrim, SpecialSymbols) {
		std::map<string, string> variants = {
			{"\n .a \n", "\n .a"},
			{"\n abc;\t\v\f  \r\t", "\n abc;"},
			{"\0 abc \n", "\0 abc"}
		};

		for (const auto& [key, value] : variants)
		{
			string value = key;
			StringExtension::RightTrim(value);
			ASSERT_EQ(value, value);
		}
	}

	TEST(Trim, EmptyString) {
		string value = "";
		StringExtension::Trim(value);
		ASSERT_EQ(value, "");
	}

	TEST(Trim, StandardUsing) {
		std::map<string, string> variants = {
			{"abc", "abc"},
			{" abc ", "abc"},
			{"\n abc \n", "abc"}
		};

		for (const auto& [key, value] : variants)
		{
			string value = key;
			StringExtension::Trim(value);
			ASSERT_EQ(value, value);
		}
	}

	TEST(Trim, SpecialSymbols) {
		std::map<string, string> variants = {
			{"\n .a \n", ".a"},
			{"\n abc;\t\v\f  \r\t", "abc;"},
			{"\0 abc \n", "abc"}
		};

		for (const auto& [key, value] : variants)
		{
			string value = key;
			StringExtension::Trim(value);
			ASSERT_EQ(value, value);
		}
	}

	TEST(ToLower, EmptyString) {
		string value = "";
		StringExtension::ToLower(value);
		ASSERT_EQ(value, "");
	}

	TEST(ToLower, StandardUsing) {
		std::map<string, string> variants = {
			{"AbC", "abc"},
			{" ABC8e! ", " abc8e! "},
			{"\n %ABC^ \n", "\n %abc^ \n"}
		};

		for (const auto& [key, value] : variants)
		{
			string value = key;
			StringExtension::ToLower(value);
			ASSERT_EQ(value, value);
		}
	}

	TEST(ToLower, SpecialSymbols) {
		std::map<string, string> variants = {
			{"\n .ABCE987 \n", "\n .abce987 \n"},
			{"\n ABC;\t\v\f  \r\t", "\n abc;\t\v\f  \r\t"},
			{"\0 ZXP \n", "\0 zxp \n"}
		};

		for (const auto& [key, value] : variants)
		{
			string value = key;
			StringExtension::ToLower(value);
			ASSERT_EQ(value, value);
		}
	}

	TEST(Split, EmptyString) {
		string value = "";
		ASSERT_THAT(Split(value, " "), ElementsAre(""));
	}

	TEST(Split, EmptySeparator) {
		string value = "ab ef";
		ASSERT_THAT(Split(value, ""), ElementsAre("a", "b", " ", "e", "f"));
	}

	TEST(Split, WeirdSeparator) {
		string value = "ab\n\23ef\n,\23mg";
		ASSERT_THAT(Split(value, "\n\23"), ElementsAre("ab", "ef\n,\23mg"));
	}

	TEST(Split, LongSeparator) {
		string separator = "";
		for (u64 i = 0; i < 10000; ++i)
			for (char c = 'A'; c <= 'z'; ++c)
				separator += c;
		string value = "1";
		value += separator;
		value += "2";
		value += separator;
		value += "3";
		value += separator;
		ASSERT_THAT(Split(value, separator), ElementsAre("1", "2", "3", ""));
	}

	TEST(Split, StandardUsing) {
		std::map<string, std::vector<string>> variants = {
			{"ab ef", {"ab", "ef"}},
			{" ABC8e! Something here.", {"", "ABC8e!", "Something", "here."}},
			{"\n %ABC^ \n", {"\n", "%ABC^", "\n"}},
			{"abcd", {"abcd"}}
		};

		for (const auto& [key, value] : variants)
			ASSERT_THAT(Split(key, " "), ElementsAreArray(value.data(), value.size()));
	}

	TEST(SplitStd, EmptyString) {
		string value = "";
		ASSERT_THAT(SplitStd(value), ElementsAre());
	}

	TEST(SplitStd, WeirdString) {
		string value = "\20 abc\n\t\v  gmce*90;. acv";
		ASSERT_THAT(SplitStd(value), ElementsAre("\20", "abc", "gmce*90;.", "acv"));
	}

	TEST(SplitStd, StandardUsing) {
		string value = "abc def\nghij klmn";
		ASSERT_THAT(SplitStd(value), ElementsAre("abc", "def", "ghij", "klmn"));
	}
}
