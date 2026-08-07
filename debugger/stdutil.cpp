#include "stdafx.h"
#include <filesystem>
namespace fs = std::filesystem;

int atoi(const std::string& s) {
	return atoi(s.c_str());
}

double atof(const std::string& s) {
	return atof(s.c_str());
}

std::string itoa(int n) {
	char buff[32]; itoa(n, buff, 10);
	return std::string(buff);
}

/////////////
//By FLOYD!//
/////////////
std::string ftoa(float n) {

	static const int digits = 6;

	int eNeg = -4, ePos = 8;	// limits for e notation.

	char buffer[50]; // from MSDN example, 25 would probably suffice
	std::string t;
	int dec, sign;

	if(_finite(n)) {
		t = _ecvt(n, digits, &dec, &sign);

		if(dec <= eNeg + 1 || dec > ePos) {

			_gcvt(n, digits, buffer);
			t = buffer;
			return t;
		}

		// Here is the tricky case. We want a nicely formatted
		// number with no e-notation or multiple trailing zeroes.

		if(dec <= 0) {

			t = "0." + std::string(-dec, '0') + t;
			dec = 1;	// new location for decimal point

		}
		else if(dec < digits) {

			t = t.substr(0, dec) + "." + t.substr(dec);

		}
		else {

			t = t + std::string(dec - digits, '0') + ".0";
			dec += dec - digits;

		}

		// Finally, trim off excess zeroes.

		int dp1 = dec + 1, p = t.length();
		while(--p > dp1 && t[p] == '0');
		t = std::string(t, 0, ++p);

		return sign ? "-" + t : t;

	}	// end of finite case

	if(_isnan(n))	return "NaN";
	if(n > 0.0)		return "Infinity";
	if(n < 0.0)		return "-Infinity";

	abort();
}

std::string tolower(const std::string& s) {
	std::string t = s;
	for(int k = 0; k < t.size(); ++k) t[k] = tolower(t[k]);
	return t;
}

std::string toupper(const std::string& s) {
	std::string t = s;
	for(int k = 0; k < t.size(); ++k) t[k] = toupper(t[k]);
	return t;
}

std::string fullfilename(const std::string& t) {
	try {
		return fs::absolute(fs::path(t)).string();
	}
	catch (const fs::filesystem_error&) {
		return t;
	}
}

std::string filenamepath(const std::string& t) {
	try {
		return fs::path(t).parent_path().string();
	}
	catch (const fs::filesystem_error&) {
		return "";
	}
}

std::string filenamefile(const std::string& t) {
	try {
		return fs::path(t).filename().string();
	}
	catch (const fs::filesystem_error&) {
		return "";
	}
}