#include <iostream>
#include <string>
#include "parser.h"
#include <sys/types.h>
#include <unistd.h>

using namespace std;

struct Command
{
};

// We alreayd know we're in the pipeline here
void runPipeline(const parsed_input* input)
{
    return;

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

        auto pid = fork();
        if (pid < 0)
        {
            cout << "FORK ERROR" << endl;
            exit(-1);
        }

        if (pid == 0)
        {
            // Child
        }
        else
        {
            // Parent
        }
    }
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

        if (!parse_success)
        {
            cout << "PARSE ERROR. LINE: " << inputLine << endl;
            exit(-1);
        }

        pretty_print(ptr);

        auto inputCount = ptr->num_inputs;
        if (inputCount <= 0)
        {
            cout << "UNEXPECTED INPUT COUNT: " << inputCount << endl;
            exit(-1);
        }

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
        default:
            {
                exit(-1);
            }
        }

        cout << "InputCount: " << inputCount << endl;

        // cout << "Received Line: " << inputLine << endl;

        cout << "/> ";
        getline(cin, inputLine);
    }

    return 0;
}