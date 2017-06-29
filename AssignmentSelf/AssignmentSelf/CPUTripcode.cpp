#include <conio.h>  
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <openssl/des.h>
#include <string>
#include "Util.h"
#include <ctime>
#include <fstream>
#include <chrono>
#include <thread>
#include <iostream>

#define ESC 0x1B

const std::string resultsFileName = "./results.txt";

std::string getTrip(char *capCode);

const char *salt = "................................"
".............../0123456789ABCDEF"
"GABCDEFGHIJKLMNOPQRSTUVWXYZabcde"
"fabcdefghijklmnopqrstuvwxyz....."
"................................"
"................................"
"................................"
"................................";

void saveResult(const std::string& result , const std::string& input) {
	std::ofstream logFile(resultsFileName, std::ios_base::out | std::ios_base::app);
	logFile << "!" << result << "        #" << input << '\n';
}

int main(int argc, char** argv) {
	srand(time(NULL));
	if (argc < 2) {
		fprintf(stderr, "Usage: %s tripcodes\n", argv[0]);
		return 1;
	}

	//Let's hope we just do one otherwise it'll take quite a while

	for (int i = 1; i < argc; i++) {
		int found = 0;
		std::string desired = argv[i];
		std::string input;
		std::string result;
		while (true) {
			if (_kbhit()) {
				char c = _getch();
				if (c == ESC) {
					break;
				}
			}

			input = RandomString(8);
			result = getTrip(CharAdapter(input.c_str()));
			if (result.find(desired) != std::string::npos) {
				saveResult(result, input);
				found++;
				std::cout << "!" << result << "        #" << input << '\n';
			}
		}
		printf("Found %i tripcodes", found);
		return 0;
	}
	return 0;
}

// https://github.com/Cairnarvon/triptools
std::string getTrip(char *capcode)
{
	char s[3], c_ret[14], *trip = c_ret + 3, *sjis, cap[9];
	int k;

	int/* i,*/ j;
	size_t l;

	l = strlen(capcode);
	sjis = capcode;

	memset(cap, 0, 9);

	for (j = k = 0; j < 8 && k < 8; ++j, ++k) {
		switch (sjis[j]) {
		case '&':
			if (sjis[j + 1] != '#') {
				cap[k] = '&';
				if (++k < 8) cap[k] = 'a';
				if (++k < 8) cap[k] = 'm';
				if (++k < 8) cap[k] = 'p';
				if (++k < 8) cap[k] = ';';
			}
			break;
		case '"':
			cap[k] = '&';
			if (++k < 8) cap[k] = 'q';
			if (++k < 8) cap[k] = 'u';
			if (++k < 8) cap[k] = 'o';
			if (++k < 8) cap[k] = 't';
			if (++k < 8) cap[k] = ';';
			break;
		case '\'':
			cap[k] = '&';
			if (++k < 8) cap[k] = '#';
			if (++k < 8) cap[k] = '0';
			if (++k < 8) cap[k] = '3';
			if (++k < 8) cap[k] = '9';
			if (++k < 8) cap[k] = ';';
			break;
		case '<':
			cap[k] = '&';
			if (++k < 8) cap[k] = 'l';
			if (++k < 8) cap[k] = 't';
			if (++k < 8) cap[k] = ';';
			break;
		case '>':
			cap[k] = '&';
			if (++k < 8) cap[k] = 'g';
			if (++k < 8) cap[k] = 't';
			if (++k < 8) cap[k] = ';';
			break;
		default:
			cap[k] = sjis[j];
		}
	}

	cap[k > 8 ? 8 : k] = 0;
	l = strlen(cap);

	s[0] = l > 1 ? salt[(unsigned char)cap[1]] : l > 0 ? 'H' : '.';
	s[1] = l > 2 ? salt[(unsigned char)cap[2]] : l > 1 ? 'H' : '.';

	DES_fcrypt(cap, s, c_ret);
	trip[10] = 0;

	return trip;
}
