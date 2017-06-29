#pragma once
#include <string>

class MyLogger {
public:
	MyLogger();
	void Clear() const;
	void Write(const std::string& text) const;
	void SetFile(const std::string &fileName);

private:
	std::string file = "";
};
