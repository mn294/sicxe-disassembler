#include "loader.h"
#include "utils.h"
#include <iostream>
#include <fstream>
#include <map>
#include <vector>
#include <string>
using namespace std;
// ===== LINKER / LOADER GLOBALS =====

// External Symbol Table: symbol → absolute address
map<string, int> ESTAB;
map<int, unsigned char> MEMORY;   //maps memory address to byte value
//unsigned char is used to represent a byte of memory (0-255)
struct ModRec { int addr; int len; string symbol; };
vector<ModRec>AllModRecs;
// Helper function to print memory map
void printMemoryMap(const map<int,unsigned char>& MEMORY, int startAddr, int endAddr) {
    cout << "\nMemory Map (Addresses " << intTohex(startAddr, 4)
         << " - " << intTohex(endAddr, 4) << "):\n";
    cout << "--------------------------------------\n";

    int count = 0;
    for (int addr = startAddr; addr <= endAddr; addr++) {
        cout << intTohex(addr,4) << ": ";
     if (MEMORY.count(addr))
       cout << intTohex(MEMORY.at(addr),2);
     else{
        cout << "??";
       }
     
     cout << "  ";
        count++;
        if (count % 8 == 0) cout << endl;//print 8 bytes per line
    }
    cout << endl << "--------------------------------------\n";
}
int findFirstFreeAddress(int programLength) {
    int addr = 0x1000;

    while (true) {
        bool free = true;

        for (int i = 0; i < programLength; i++) {
            if (MEMORY.count(addr + i)) {
                free = false;
                addr = addr + i + 1;
                break;
            }
        }

        if (free) return addr;
    }
}

// // Loader/Linker
void runLoader() {
     int LoadAddr = 0;
    int numPrograms;
    cout << "Enter number of programs to link/load: ";
    cin >> numPrograms;
    cin.ignore();

    vector<int> loadAddrs; // store starting addresses per program

    for (int p = 0; p < numPrograms; p++) {
        string filename;
        cout << "\nEnter filename of program #" << (p + 1) << ": ";
        cin >> filename;

        // Prepend "examples/" if not already included
        if (filename.find("examples/") != 0 && filename.find("examples\\") != 0) {
            filename = "examples/" + filename;
        }

        ifstream infile(filename);
        if (!infile) {
            cout << "Error: Cannot open file " << filename << endl;
            return;
        }

        vector<TRecord> localT;
        vector<ModRec> localM;
        vector<string> extrefs;
        map<string, int> localESTAB;
        string progName;
        int progLength = 0;

        // ===== READ OBJECT FILE (HTE) =====
        string line;
        while (getline(infile, line)) {
            if (line.empty()) continue;
            vector<string> f = split(line, '^');

            if (f[0] == "H") {
                progName = f[1];
                progLength = stoi(f[3], nullptr, 16);
            }
            else if (f[0] == "D") {
                // EXTDEF
                for (size_t i = 1; i < f.size(); i += 2) {
                    string sym = f[i];
                    int relAddr = stoi(f[i+1], nullptr, 16);
                    localESTAB[sym] = relAddr; // relative, relocate later
                }
            }
            else if (f[0] == "R") {
                // EXTREF
                for (size_t i = 1; i < f.size(); i++)
                    extrefs.push_back(f[i]);
            }
            else if (f[0] == "T") {
                TRecord tr;
                tr.start = stoi(f[1], nullptr, 16);
                tr.length = stoi(f[2], nullptr, 16);
                tr.obj = f[3];
                localT.push_back(tr);
            }
            else if (f[0] == "M") {
                ModRec m;
                m.addr = stoi(f[1], nullptr, 16);
                m.len = stoi(f[2], nullptr, 16);
                m.symbol = (f.size() > 3) ? f[3] : "";
                localM.push_back(m);
                AllModRecs.push_back(m);
            }
            else if (f[0] == "E") {
                break;
            }
        }
        infile.close();

        // ===== ASSIGN RANDOM GLOBAL LOAD ADDRESS =====
         LoadAddr = findFirstFreeAddress(progLength);
    for (int i = 0; i < progLength; i++) {
         MEMORY[LoadAddr + i] = MEMORY.count(LoadAddr + i)
                           ? MEMORY[LoadAddr + i]
                           : 0x00;
                   }
        cout << "Program " << progName << " assigned load address: " << intTohex(LoadAddr,4) << endl;
        loadAddrs.push_back(LoadAddr);
        // ===== RELOCATE LOCAL ESTAB =====
        for (auto &e : localESTAB)
            e.second += LoadAddr;

        // Merge local ESTAB into global ESTAB
        for (auto &e : localESTAB)
            ESTAB[e.first] = e.second;

        // ===== LOAD TEXT RECORDS =====
        for (auto &tr : localT) {
            int mem = LoadAddr + tr.start;
            for (size_t i = 0; i < tr.obj.length(); i += 2) {
                MEMORY[mem++] = stoi(tr.obj.substr(i, 2), nullptr, 16);
            }
        }



        // ===== PRINT MEMORY MAP FOR THIS PROGRAM =====
        cout << "\nMemory Map after loading program " << progName << ":\n";
        printMemoryMap(MEMORY, LoadAddr, LoadAddr + progLength - 1);
    } // end of all programs loop
            // ===== APPLY MODIFICATION RECORDS =====
        for (auto &m : AllModRecs) {
            int addr = LoadAddr + m.addr;
            int len = m.len;

            int nibbles = len;
            int totalBits = nibbles * 4;
            int totalBytes = (totalBits + 7) / 8;

            // Read bytes
            int value = 0;
            for (int i = 0; i < totalBytes; i++)
            {
                value = (value << 8) | MEMORY[addr + i];
            }

            // Mask to keep only field bits
            int fieldMask = (1 << totalBits) - 1;
            int fieldValue = value & fieldMask;

            // Apply relocation
            fieldValue += ESTAB[m.symbol];

            // Merge back
            value = (value & ~fieldMask) | (fieldValue & fieldMask);

            // Write back
            for (int i = totalBytes - 1; i >= 0; i--)
            {
                MEMORY[addr + i] = value & 0xFF;
                value >>= 8;
            }
        }
    // ===== PRINT FINAL COMBINED MEMORY MAP =====
    int startAddr = loadAddrs.front();
    int endAddr = MEMORY.rbegin()->first;
    cout << "\n==== FINAL COMBINED MEMORY MAP ====\n";
    printMemoryMap(MEMORY, startAddr, endAddr);

    // ===== PRINT FINAL ESTAB =====
    cout << "\n==== EXTERNAL SYMBOL TABLE (ESTAB) ====\n";
    cout << "Symbol\tAddress\n";
    cout << "----------------\n";
    for (auto &e : ESTAB)
        cout << setw(10) << left << e.first
             << intTohex(e.second, 4) << endl;
}