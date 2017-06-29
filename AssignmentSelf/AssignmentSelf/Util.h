#pragma once
#include <cstdlib>
#include <string.h>
#include <algorithm>

class CharAdapter
{
public:
	CharAdapter(const char* s) : m_s(::_strdup(s)) { }
	CharAdapter(const CharAdapter& other) = delete; // non construction-copyable
	CharAdapter& operator=(const CharAdapter&) = delete; // non copyable

	~CharAdapter() /*free memory on destruction*/
	{
		::free(m_s); /*use free to release strdup memory*/
	}
	operator char*() /*implicit cast to char* */
	{
		return m_s;
	}

private:
	char* m_s;
};

//http://stackoverflow.com/questions/440133/how-do-i-create-a-random-alpha-numeric-string-in-c
std::string RandomString(size_t length)
{
	auto randchar = []() -> char
	{
		const char charset[] =
			"0123456789"
			"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
			"abcdefghijklmnopqrstuvwxyz"
			"./?";
		const size_t max_index = (sizeof(charset) - 1);
		return charset[rand() % max_index];
	};
	std::string str(length, 0);
	std::generate_n(str.begin(), length, randchar);
	return str;
}