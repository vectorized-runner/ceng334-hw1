#include <iostream>
#include <string>
#include "parser.h"
#include <sys/types.h>
#include <unistd.h>
#include <vector>

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


void closeFile(int fd){
    auto result = close(fd);
    assert(result >= 0, "close error");
}

void waitForChildProcess(pid_t pid){
    int status;
    waitpid(pid, &status, 0);

    if(WIFEXITED(status)){
        if(WEXITSTATUS(status) < 0){
            cout << "Child exited with ERROR status: " << WEXITSTATUS(status) << endl;
        }
    } else if(WIFSIGNALED(status)){
        cout << "Child exited with signal: " << WTERMSIG(status) << endl;
    }
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

void runProgram(char* args[]){
    execvp(args[0], args);

    // Execvp shouldn't return
    assert(false, "execvp error");
}

// We alreayd know we're in the pipeline here
void runPipeline(const parsed_input* input)
{
    vector<pid_t> childPids;
    auto inputCount = (int)input->num_inputs;

    int pipeCount = inputCount - 1;
    int* pipeWriteFds = new int[pipeCount];
    int* pipeReadFds = new int[pipeCount];

    cout << "InputC" << inputCount << endl;

    // Example: A | B | C
    // F1: OG/A, F2: OG/B, F3: OG/C (Requires 3 fork)
    // P1: A->B, P2: B->C (Requires 2 pipe)
    for (int i = 0; i < inputCount; i++)
    {
        auto currentCommand = input->inputs[i];
        auto type = currentCommand.type;
        assert(type == INPUT_TYPE_COMMAND, "unexpected input");
  
        if(i != inputCount - 1){
            cout << "Create pipe at index" << i << endl;
            pipe(pipeReadFds[i], pipeWriteFds[i]);
            assert(pipeReadFds[i] > 0, "piperead");
            assert(pipeWriteFds[i] > 0, "pipewrite");
        }

        bool isChild;
        pid_t childPid;
        fork(isChild, childPid);

        int pipefd[2];
        pipe(pipefd);

        if(isChild){
            // Redirect A -> B, B -> C, Run A, B, C
   
            if(i != 0){
                // B listens from A
                // C listens from B
                // cout << "redirect stdin: " << (i - 1) << endl;
                // redirectStdin(pipeReadFds[i - 1]);

                close(pipefd[1]);
        
                    // Duplicate the read end of the pipe onto stdin
                dup2(pipefd[0], STDIN_FILENO);
        
                // Close the original read end of the pipe
                close(pipefd[0]);
            }

            // A writes to B
            // B writes to C
            if(i != inputCount - 1){
                // cout << "redirect stdout: " << i << endl;
                // redirectStdout(pipeWriteFds[i]);


                close(pipefd[0]);
                dup2(pipefd[1], STDOUT_FILENO);
                close(pipefd[1]);
            }

            auto programArgs = currentCommand.data.cmd.args;
            // Notice program a doesn't continue after here
            runProgram(programArgs);

            cout << "Shouldn't reach here" << endl;
        } else{
            cout << "Create Child: " << childPid << endl;
            // OG Process
            childPids.push_back(childPid);
        }
    }

    auto childCount = (int)childPids.size();
    for(int i = 0; i < childCount; i++){
        waitForChildProcess(childPids[i]);
        cout << "One child process exited: " << childPids[i] << endl;
    }

    cout << "Hell yeah." << endl;     
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