#include <iostream>
#include <string>
#include "parser.h"

using namespace std;

int main() {

    string inputLine;
    getline(cin, inputLine);

    while(inputLine != "quit"){
        /*
        parsed_input* ptr = (parsed_input*)malloc(sizeof(parsed_input)); 
        auto cPtr = const_cast<char*>(inputLine.c_str());
        auto parse_success = parse_line(cPtr, ptr);

        if(!parse_success){
            cout << "Parse error for line: " << inputLine << endl;
        }
        */

       cout << "Received Line: " << inputLine << endl;
    }

    return 0;
}