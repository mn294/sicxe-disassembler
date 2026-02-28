#include <iostream>
#include "loader.h"
#include "disassembler.h"
using namespace std;

int main() {
    int choice;
    cout << "Select mode:\n";
    cout << "1. SIC/XE Disassembler\n";
    cout << "2. Linker / Loader \n";
    cout << "Choice: ";
    cin >> choice;
    cin.ignore();//to ignore the newline character after the integer input
    if (choice == 1)
        runDisassembler();
    else if (choice == 2)
        runLoader();
    else
        cout << "Invalid choice.\n";

    return 0;
}