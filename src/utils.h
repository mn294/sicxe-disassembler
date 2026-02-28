#ifndef UTILS_H
#define UTILS_H

#include <string>
#include <vector>

struct TRecord {
	int start;
	int length;
	std::string obj;
};

std::vector<std::string> split(const std::string&, char);
std::string intTohex(int, int);

#endif