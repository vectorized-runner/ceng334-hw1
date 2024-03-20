#include <iostream>
#include <string>
#include "parser.h"
#include <sys/types.h>
#include <unistd.h>

using namespace std;

void assert(bool condition, string message){
    if(!condition){
        cout << "ASSERTION FAILED: " << message << endl;
        exit(-1);
    }
}

void fork(bool& isChild, pid_t& childPid){
    auto pid = fork();
    assert(pid >= 0, "fork");
    isChild = pid == 0;

    if(!isChild){
        childPid = pid;
    }
}

void pipe(int& read, int& write){
    int fd[2];
    int result = pipe(fd);
    assert(result >= 0, "pipe error");
    read = fd[0];
    write = fd[1];
}

void waitForChildProcess(pid_t pid){
    int status;
    waitpid(pid, &status, 0);

    if(WIFEXITED(status) < 0){
        cout << "Child exited with status: " << WEXITSTATUS(status) << endl;
    } else if(WIFSIGNALED(status)){
        cout << "Child exited with signal: " << WTERMSIG(status) << endl;
    }
}

void runProgram(char* args[]){
    execvp(args[0], args);

    // Execvp shouldn't return
    assert(false, "execvp error");
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
        pid_t pid;
        fork(isChild, pid);

        if(isChild){
            cout << "Running on Child" << endl;
            exit(0);
        } else{
            cout << "Running on Parent" << endl;
            exit(0);
        }
    }
}

void closeFile(int fd){
    auto result = close(fd);
    assert(result >= 0, "close error");
}

void redirectStdout(int writeFd){
    auto result = dup2(writeFd, STDOUT_FILENO);
    assert(result >= 0 , "dup error");
    closeFile(writeFd);
}

void redirectStdin(int readFd){
    auto result = dup2(readFd, STDIN_FILENO);
    assert(result >= 0 , "dup error");
    closeFile(readFd);
}

void runSingleCommand(parsed_input* input){
    assert(input->num_inputs == 1, "numinputs");
    auto type = input->inputs[0].type;
    assert(type == INPUT_TYPE_COMMAND, "inputtype");

    bool isChild;
    pid_t childPid;
    fork(isChild, childPid);

    if(isChild){
        auto args = input->inputs[0].data.cmd.args;
        
        // Child process inherits the stdout from parent, no need for redirection
        runProgram(args);
    } else{
        waitForChildProcess(childPid);
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
            runSingleCommand(ptr);
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