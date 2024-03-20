#include <iostream>
#include <string>
#include "parser.h"
#include <sys/types.h>
#include <unistd.h>

using namespace std;

struct Command
{
};

void assert(bool condition, string message){
    if(!condition){
        cout << "ASSERTION FAILED: " << message << endl;
        exit(-1);
    }
}

void Fork(bool& isChild){
    auto pid = fork();
    assert(pid >= 0, "fork");
    isChild = pid == 0;
}

// We alreayd know we're in the pipeline here
void runPipeline(const parsed_input* input)
{
    auto inputCount = (int)input->num_inputs;

    // Remember: Single separator
    for (int i = 0; i < inputCount; i++)
    {
        auto currentCommand = input->inputs[i];
        auto type = currentCommand.type;

        switch (type)
        {
            case INPUT_TYPE_SUBSHELL:
                // TODO: Implement Later
                break;
            case INPUT_TYPE_COMMAND:
                // TODO: Implement Now.
                break;
            case INPUT_TYPE_PIPELINE:
                // TODO: Implement Later
                break;
            default:
            {
                cout << "COMMAND TYPE ERROR" << endl;
                exit(-1);
            }
        }


        bool isChild;
        Fork(isChild);

        if(isChild){
            cout << "Running on Child" << endl;
            exit(0);
        } else{
            cout << "Running on Parent" << endl;
            exit(0);
        }
    }
}

void executeSingleCommand(parsed_input* input){
    assert(input->num_inputs == 1, "numinputs");
    auto type = input->inputs[0].type;
    assert(type == INPUT_TYPE_COMMAND, "inputtype");

    auto pid = fork();
    assert(pid >= 0, "fork");

    // TODO: Execute single command
    cout << "Exec single command." << endl; 
}

int main()
{
    string inputLine;

    cout << "/> ";
    getline(cin, inputLine);

    while (inputLine != "quit")
    {
        parsed_input* ptr = (parsed_input*)malloc(sizeof(parsed_input));
        auto cPtr = const_cast<char *>(inputLine.c_str());
        auto parse_success = parse_line(cPtr, ptr);

        assert(parse_success, "parse error");

        pretty_print(ptr);

        auto inputCount = ptr->num_inputs;

        assert(inputCount > 0, "inputCount");

        auto separator = ptr->separator;

        // TODO:
        switch (separator)
        {
        case SEPARATOR_PARA:
            break;
        case SEPARATOR_PIPE:
        {
            runPipeline(ptr);
            break;
        }
        case SEPARATOR_SEQ:
            break;
        case SEPARATOR_NONE:
        {
            executeSingleCommand(ptr);
            break;
        }
        default:
            {
                cout << "UNEXPECTED SEPARATOR" << endl;
                exit(-1);
            }
        }

        // cout << "InputCount: " << inputCount << endl;
        // cout << "Received Line: " << inputLine << endl;

        cout << "/> ";
        getline(cin, inputLine);
    }

    return 0;
}