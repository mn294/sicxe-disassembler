#include "disassembler.h"
#include "utils.h"
#include <iostream>
#include <string>
#include <set>
#include <fstream>
#include<map>
using namespace std;

int BASE = -1; // -1 means BASE is not set yet
struct Opcode {
   string mnemonic;
    int format;   // 1, 2, or 3 (3 may become 4)
};
int PROG_START = 0;
int EXEC_ADDR = 0;
int labelCount = 0;
map<int, Opcode> OPTAB;// opcode → {mnemonic, format}
map<int, string> SYMTAB;   // address → label
set<int> referencedAddrs;
map<int, int> MMap; // address -> length
vector<TRecord> TRecords;//store T records to be used in pass 2
void initOPTAB() {
    OPTAB[0x18] = {"ADD", 3};
    OPTAB[0x58] = {"ADDF", 3};
    OPTAB[0x40] = {"AND", 3};
    OPTAB[0x28] = {"COMP", 3};
    OPTAB[0xA0] = {"COMPR", 2};
    OPTAB[0x88] = {"COMPF", 3};
    OPTAB[0x24] = {"DIV", 3};
    OPTAB[0x64] = {"DIVF", 3};
    OPTAB[0x3C] = {"J", 3};
    OPTAB[0x30] = {"JEQ", 3};
    OPTAB[0x34] = {"JGT", 3};
    OPTAB[0x38] = {"JLT", 3};
    OPTAB[0x48] = {"JSUB", 3};
    OPTAB[0x00] = {"LDA", 3};
    OPTAB[0x68] = {"LDB", 3};
    OPTAB[0x50] = {"LDCH", 3};
    OPTAB[0x08] = {"LDL", 3};
    OPTAB[0x6C] = {"LDS", 3};
    OPTAB[0x74] = {"LDT", 3};
    OPTAB[0x04] = {"LDX", 3};
    OPTAB[0x1C] = {"SUB", 3};
    OPTAB[0x0C] = {"STA", 3};
    OPTAB[0x54] = {"STCH", 3};
    OPTAB[0x14] = {"STL", 3};
    OPTAB[0x10] = {"STX", 3};
    OPTAB[0xB4] = {"CLEAR", 2};
    OPTAB[0xE0] = {"TD", 3};
    OPTAB[0xD8] = {"RD", 3};
    OPTAB[0x4C] = {"RSUB", 3};
    OPTAB[0xDC] = {"WD", 3};
    OPTAB[0x2C] = {"TIX", 3};
    OPTAB[0xB8] = {"TIXR", 2};
    OPTAB[0xC4] = {"FIX", 1};
    OPTAB[0xC0] = {"FLOAT", 1};
    OPTAB[0xF4] = {"HIO", 1};
    OPTAB[0xF0] = {"SIO", 1};
    OPTAB[0xF8] = {"TIO", 1};
}
map<int, string> REGTAB = {{0, "A"}, {1, "X"}, {2, "L"}, {3, "B"}, {4, "S"}, {5, "T"}, {6, "F"}, {8, "PC"}, {9, "SW"}};



void printLine(ostream& out, int loc, string label, string instr,string operand, string obj, bool reloc = false) {//we had to set reloc to false as default value to avoid changing all previous calls to printLine
    if (loc != -1) {
        out <<setw(6)<<setfill(' ')<< uppercase << hex << loc << "\t";
    } else {
        out <<setw(6)<< " ";
    }
    out << setw(10) << left << label
        << setw(8)  << left << instr
        << setw(12) << left << operand
         << obj;
         if (reloc) out<<" *M";
         out << endl;
}
bool looksLikeInstruction(int opcode, size_t index, const string &obj) {
    if (!OPTAB.count(opcode))
        return false;

    int fmt = OPTAB[opcode].format;

    if (fmt == 1)
        return true;

    if (fmt == 2)
        return index + 4 <= obj.length();

    if (index + 6 > obj.length())
        return false;

    int byte2 = stoi(obj.substr(index + 2, 2), nullptr, 16);
    bool e = byte2 & 0x10;
    bool b = byte2 & 0x20;
    bool p = byte2 & 0x40;
    if(b && p) return false;//both b and p cant be 1 at the same time

    return e ? (index + 8 <= obj.length()) : true;//if e is true check that we have 8 hex digits,else(e=0) means that it's format 3 and we already checked its length
}

bool isPrintableASCIIByte(int val) { 
    return (val >= 0x20 && val <= 0x7E);
}

int looksLikeByte(const string &obj, size_t index, int LOCCTR, ofstream &asmOut) {
    string chars = "";
    string hexBytes = "";
    size_t i = index;

    while (i + 2 <= obj.length()) {
        int byteVal = stoi(obj.substr(i, 2), nullptr, 16);

        if (!isPrintableASCIIByte(byteVal))
            break;

        chars += char(byteVal);
        hexBytes += obj.substr(i, 2);
        i += 2;
    }

    // If we found at least ONE printable character
    if (!chars.empty()) {
        string label = SYMTAB.count(LOCCTR) ? SYMTAB[LOCCTR] : SYMTAB[LOCCTR] = "L" + intTohex(labelCount++, 3);
        printLine(asmOut, LOCCTR, label, "BYTE",
                  "C'" + chars + "'", hexBytes);
        return chars.length(); // bytes consumed
    }

    // Otherwise → BYTE X'
    string oneByte = obj.substr(index, 2);
    string label = SYMTAB.count(LOCCTR) ? SYMTAB[LOCCTR] : SYMTAB[LOCCTR] = "L" + intTohex(labelCount++, 3);
    printLine(asmOut, LOCCTR, label, "BYTE",
              "X'" + oneByte + "'", oneByte);
    return 1;
}
int looksLikeWord(const string &obj, size_t index, int LOCCTR, ofstream &asmOut) {
    if (index + 6 > obj.length())
        return 0; // safety check

    string hexWord = obj.substr(index, 6);
    int value = stoi(hexWord, nullptr, 16);
    string label = SYMTAB.count(LOCCTR) ? SYMTAB[LOCCTR] : SYMTAB[LOCCTR] = "L" + intTohex(labelCount++, 3);
    printLine(asmOut, LOCCTR, label, "WORD",
              to_string(value), hexWord);

    return 3; // bytes consumed
}
//Disassembler Code
 int runDisassembler(){
    initOPTAB();
    string basePath="examples/";
    ifstream input(basePath+"in.txt");//ifstream input is a file input stream object to read from a file named "in.txt"

    if (!input) {
        cout << "Error: Cannot open input file.\n";
        return 1;
    }
    ofstream asmOut(basePath+"assembly.txt");
    if (!asmOut) {
    cout << "Error: Cannot open assembly.txt\n";
    return 1;
  }
  ofstream symOut(basePath+"symbolTable.txt");
    if (!symOut) {
    cout << "Error: Cannot open symbolTable.txt\n";
    return 1;
  }

string line;//string line is a string variable to hold each line read from the file

  string programName;
int startAddress = 0;
// ================= PASS 1 =================
input.clear();
input.seekg(0);


int prevEnd = -1;
bool headerSeen = false;

while (getline(input, line)) {
    vector<string> fields = split(line, '^');//

    if (fields[0] == "H") {
        programName = fields[1];
        startAddress = stoi(fields[2], nullptr, 16);
         headerSeen = true;
    }

    else if (fields[0] == "T") {
        TRecord tr;
        tr.start = stoi(fields[1], nullptr, 16);
        tr.length = stoi(fields[2], nullptr, 16);
        tr.obj = fields[3];
        TRecords.push_back(tr);

        int LOCCTR = tr.start;
        size_t index = 0;

        // ===== GAP → RESB / RESW =====
        if (prevEnd != -1 && tr.start > prevEnd) {
            //int gap = tr.start - prevEnd;
            SYMTAB[prevEnd] = "L" + intTohex(labelCount++, 3);
        }

        while (index + 2 <= tr.obj.length()) {
            int byte1 = stoi(tr.obj.substr(index, 2), nullptr, 16);
            int opcode = byte1 & 0xFC;

            if (looksLikeInstruction(opcode, index, tr.obj)) {
                int size = OPTAB[opcode].format == 1 ? 1 :
                           OPTAB[opcode].format == 2 ? 2 :
                           ((stoi(tr.obj.substr(index + 2, 2), nullptr, 16) & 0x10) ? 4 : 3);

                // collect target address for labels
                if (size >= 3) {
                    int byte2 = stoi(tr.obj.substr(index + 2, 2), nullptr, 16);
                    int disp = ((byte2 & 0x0F) << 8) |
                                stoi(tr.obj.substr(index + 4, 2), nullptr, 16);
                    int target = LOCCTR + size + disp;//PC-relative by default
                    referencedAddrs.insert(target);
                }

                index += size * 2;
                LOCCTR += size;
            }
            else {//data byte
                referencedAddrs.insert(LOCCTR);
                index += 2;
                LOCCTR += 1;
            }
        }
        prevEnd = tr.start + tr.length;
    }

    else if (fields[0] == "M") {
        int addr = stoi(fields[1], nullptr, 16);
        int len  = stoi(fields[2], nullptr, 16);
        MMap[addr] = len;
    }
    else if (fields[0] == "E") {
        EXEC_ADDR = fields.size() > 1 ? stoi(fields[1], nullptr, 16) : PROG_START;
    }
}

// ===== finalize SYMTAB =====
for (int addr : referencedAddrs) {
    if (!SYMTAB.count(addr))//check if the address is not already in the SYMTAB
        SYMTAB[addr] = "L" + intTohex(labelCount++, 3);
}
// ================= PASS 2 =================
prevEnd = -1;
if(headerSeen){
    asmOut << left
           << setw(8) << programName
           << setw(8) << "START"
           << setw(8) << hex << uppercase << startAddress
           << endl;
}
for (auto &tr : TRecords) {
    // RESB / RESW detection
    if (prevEnd != -1 && tr.start > prevEnd) {
        int gap = tr.start - prevEnd;
       if (gap % 3 == 0)
            printLine(asmOut, prevEnd,SYMTAB[prevEnd], "RESW", to_string(gap / 3), "");
        else
            printLine(asmOut, prevEnd, SYMTAB[prevEnd], "RESB", to_string(gap), "");
    }

    int LOCCTR = tr.start;
    size_t index = 0;

   while (index + 2 <= tr.obj.length())
  {
    int byte1 = stoi(tr.obj.substr(index, 2), nullptr, 16);
    int rawOpcode = byte1;
    int LOC = LOCCTR;

    string label = SYMTAB.count(LOC) ? SYMTAB[LOC] : "";
    string instr = "";
    string operand = "";
    int size = 0;
    bool reloc = false;

    // =====================================================
    // FORMAT 1 (exact opcode match — NO masking)
    // =====================================================
    if (OPTAB.count(rawOpcode) && OPTAB[rawOpcode].format == 1)
    {
        instr = OPTAB[rawOpcode].mnemonic;
        size = 1;

        printLine(asmOut, LOC, label, instr, "", tr.obj.substr(index, 2));

        index += 2;
        LOCCTR += 1;
        continue;
    }

    // =====================================================
    // MASK opcode for format 2/3/4
    // =====================================================
    int opcode = rawOpcode & 0xFC;

    // =====================================================
    // NOT AN INSTRUCTION → DATA
    // =====================================================
    if (!OPTAB.count(opcode))
    {
        int consumed = looksLikeByte(tr.obj, index, LOCCTR, asmOut);

        if (consumed == 1)
        {
            int w = looksLikeWord(tr.obj, index, LOCCTR, asmOut);
            if (w > 0) consumed = w;
        }

        index += consumed * 2;
        LOCCTR += consumed;
        continue;
    }
    else// ===== INSTRUCTION =====
    {
    Opcode op = OPTAB[opcode];
    instr = op.mnemonic;
     if(op.format == 1){
        int consumed = looksLikeByte(tr.obj, index, LOCCTR, asmOut);
        index += consumed * 2;
        LOCCTR += consumed;
        continue;
     }
    // =====================================================
    // FORMAT 2
    // =====================================================
    if (op.format == 2)
    {
        int byte2 = stoi(tr.obj.substr(index + 2, 2), nullptr, 16);
        int r1 = (byte2 & 0xF0) >> 4;
        int r2 = (byte2 & 0x0F);

        if (instr == "CLEAR" || instr == "TIXR")
            operand = REGTAB[r1];
        else
            operand = REGTAB[r1] + "," + REGTAB[r2];

        size = 2;

        printLine(asmOut, LOC, label, instr, operand,
                  tr.obj.substr(index, 4));

        index += 4;
        LOCCTR += 2;
        continue;
    }

    // =====================================================
    // FORMAT 3 / 4
    // =====================================================
    if (index + 6 > tr.obj.length())
{
    int consumed = looksLikeByte(tr.obj, index, LOCCTR, asmOut);
    index += consumed * 2;
    LOCCTR += consumed;
    continue;
}
    int byte2 = stoi(tr.obj.substr(index + 2, 2), nullptr, 16);

    int n = (byte1 & 0x02) >> 1;
    int i = (byte1 & 0x01);
    int x = (byte2 & 0x80) >> 7;
    int b = (byte2 & 0x40) >> 6;
    int p = (byte2 & 0x20) >> 5;
    int e = (byte2 & 0x10) >> 4;

    int targetAddr = 0;
    if((n==1 && i==0 && x==1) || (n==0 && i==1 && x==1))
    {
        // Invalid addressing mode, treat as data
        int consumed = looksLikeByte(tr.obj, index, LOCCTR, asmOut);
        index += consumed * 2;
        LOCCTR += consumed;
        continue;
    }
    if(n==0 && i==0 && x==0 && b==0 && p==0 && instr!="RSUB")//unlikely to be an instr as most of the prog is sic/xe
    {
       int consumed=looksLikeWord(tr.obj,index,LOCCTR,asmOut);
       index+=consumed*2;
        LOCCTR+=consumed;
        continue;
    }
     
    if (e == 1)
    {
        // ---------- FORMAT 4 ----------
        int addr =
            ((byte2 & 0x0F) << 16) |
            (stoi(tr.obj.substr(index + 4, 2), nullptr, 16) << 8) |
            stoi(tr.obj.substr(index + 6, 2), nullptr, 16);

        size = 4;
        instr = "+" + instr;
        targetAddr = addr;

        if (MMap.count(LOC))
            reloc = true;
    }
    else
    {
        // ---------- FORMAT 3 ----------
        int disp =
            ((byte2 & 0x0F) << 8) |
            stoi(tr.obj.substr(index + 4, 2), nullptr, 16);

        if (disp & 0x800)
            disp -= 0x1000;

        if (p == 1)
            targetAddr = LOCCTR + 3 + disp;
        else if (b == 1 && BASE != -1)
            targetAddr = BASE + disp;
        else
            targetAddr = disp;

        size = 3;
    }

    // Addressing mode
    if (n == 0 && i == 1) operand = "#";
    else if (n == 1 && i == 0) operand = "@";

    if (instr == "RSUB")
        operand = "";
    else if (SYMTAB.count(targetAddr))
        operand += SYMTAB[targetAddr];
    else
        operand += intTohex(targetAddr, 4);

    if (x == 1) operand += ",X";

    printLine(asmOut, LOC, label, instr, operand,
              tr.obj.substr(index, size * 2), reloc);

    // BASE directive
    if (instr == "+LDB" || instr == "LDB")
    {
        BASE = targetAddr;
        //to remove the # or @ from the operand
        if (!operand.empty() && (operand[0] == '#' || operand[0] == '@')) {
            operand = operand.substr(1);
        }
        printLine(asmOut, -1, "", "BASE", operand, "");
    }

    index += size * 2;
    LOCCTR += size;
}
  }
    prevEnd = tr.start + tr.length;
}
// END directive
printLine(asmOut, -1, "", "END",
          SYMTAB.count(EXEC_ADDR) ? SYMTAB[EXEC_ADDR] : intTohex(EXEC_ADDR, 4), "");

symOut << "Symbol\tAddress\n";
symOut << "----------------\n";

for (auto &entry : SYMTAB)
{
    symOut << entry.second << "\t"
           << intTohex(entry.first, 4) << endl;
}

    input.close();
    asmOut.close();
    symOut.close();

printf("Disassembly complete. Output written to assembly.txt and symbolTable.txt\n");
return 0;
}