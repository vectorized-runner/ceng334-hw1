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
void runPipeline(parsed_input* input)
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

        if(isChild){
            // Redirect A -> B, B -> C, Run A, B, C
   
            if(i != 0){
                // B listens from A
                // C listens from B
                // char* buf = new char[500];
                // read(pipeReadFds[i - 1], buf, 500);
                // cout << "reading from parent: " << buf << endl;
                // cout << "redirect stdin: " << (i - 1) << endl;
                close(pipeWriteFds[i - 1]);
                redirectStdin(pipeReadFds[i - 1]);
            }

            // For any process, we need to close the write-ends that's been created so far,
            // Otherwise the child process can't detect EOF
            for(int x = i - 1; x >= 0; x--){
                close(pipeWriteFds[x]);
            }

            // A writes to B
            // B writes to C
            if(i != inputCount - 1){
                // cout << "redirect stdout: " << i << endl;
                redirectStdout(pipeWriteFds[i]);
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

    // Close all write ends - otherwise can't detect EOF
    for(int i = 0; i < pipeCount; i++){
        closeFile(pipeWriteFds[i]);
    }

    auto childCount = (int)childPids.size();
    for(int i = 0; i < childCount; i++){
        cout << "Waiting for child..." << childPids[i] << endl;
        waitForChildProcess(childPids[i]);
        cout << "One child process exited: " << childPids[i] << endl;
    }

    delete[] pipeWriteFds;
    delete[] pipeReadFds;

    // cout << "Pipeline run done!" << endl;     
}

void runParallel(parsed_input* input){
    auto inputCount = (int)input->num_inputs;
    assert(inputCount > 1, "numinputs");
    
    vector<pid_t> childPids;

    for(int i = 0; i < inputCount; i++){
        bool isChild;
        pid_t childPid;
        fork(isChild, childPid);

        if(isChild){
            auto type = input->inputs[i].type;
            if(type == INPUT_TYPE_COMMAND){
                auto args = input->inputs[i].data.cmd.args;
                runProgram(args);
            } else if(type == INPUT_TYPE_PIPELINE){
                // TODO;
            } else{
                assert(false, "inputtype-runpara");
            }
        } else{
            childPids.push_back(childPid);
        }
    }

    for(int i = 0; i < inputCount; i++){
        waitForChildProcess(childPids[i]);
    }

    cout << "Parallel Run Done." << endl;
}

void runSequential(parsed_input* input){
    // Example: A ; B ; C
    // Forking: OG/A, OG/B, OG/C (3 times)
    auto inputCount = (int)input->num_inputs;
    assert(inputCount > 1, "numinputs");

    for(int i = 0; i < inputCount; i++){
        bool isChild;
        pid_t childPid;
        fork(isChild, childPid);

        if(isChild){
            assert(input->inputs[i].type == INPUT_TYPE_COMMAND, "inputtype-runseq");
            auto args = input->inputs[i].data.cmd.args;
            runProgram(args);
        } else{
            waitForChildProcess(childPid);
        }
    }

    // cout << "Sequential Run Done!" << endl;
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
        if(inputLine.empty()){
            getline(cin, inputLine);
            continue;
        }

        cout << "Running For Input: '" << inputLine << "'" << endl;

        parsed_input* ptr = (parsed_input*)malloc(sizeof(parsed_input));
        auto cPtr = const_cast<char *>(inputLine.c_str());
        auto parse_success = parse_line(cPtr, ptr);

        assert(parse_success, "parse error");

        pretty_print(ptr);

        auto inputCount = ptr->num_inputs;

        assert(inputCount > 0, "inputCount");

        auto separator = ptr->separator;

        switch (separator)
        {
        case SEPARATOR_PARA:
        {
            runParallel(ptr);
            break;
        }
        case SEPARATOR_PIPE:
        {
            runPipeline(ptr);
            break;
        }
        case SEPARATOR_SEQ:
        {
            runSequential(ptr);
            break;
        }
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

        cout << "/> ";
        getline(cin, inputLine);
    }

    return 0;
}