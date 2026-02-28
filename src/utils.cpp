#include "utils.h"
#include <sstream>
#include <iomanip>
using namespace std;

vector<string> split(const string& s, char delimiter) {//function to split a string by a given delimiter
    vector<string> tokens;
    string token = "";

    for (char c : s) {
        if (c == delimiter) {
            tokens.push_back(token);
            token = "";//reset token for the next segment(input line)
        } else {
            token += c;
        }
    }
    tokens.push_back(token);
    return tokens;//returns the vector of split strings
}
// convert integer to uppercase hex string padded with zeros to given width (number of hex digits)
string intTohex(int value, int width) {
    stringstream ss;
    ss << uppercase << hex << setw(width) << setfill('0') << (unsigned int)value;//width is the minimum number of characters to be written to the output stream
    return ss.str();
}