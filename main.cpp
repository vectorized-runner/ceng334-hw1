#include <iostream>
#include <string>
#include "parser.h"
#include <sys/types.h>
#include <unistd.h>
#include <vector>
#include <fcntl.h>
#include <sys/wait.h>

using namespace std;

struct SubshellArgs{
    char str[INPUT_BUFFER_SIZE];
};

struct CommandArgs{
    char* args[MAX_ARGS];
};

struct CommandSubshellArgs{
    bool isCommand;
    CommandArgs commandArgs;
    SubshellArgs subshellArgs;
};

struct PipelineArgs {
    CommandSubshellArgs commands[MAX_INPUTS];
    int count;
};

void assert(bool condition, string message){
    if(!condition){
        cout << "ASSERTION FAILED: " << message << endl;
        exit(-1);
    }
}

parsed_input* parseInput(char* str){
    parsed_input* ptr = (parsed_input*)malloc(sizeof(parsed_input));
    auto parse_success = parse_line(str, ptr);
    assert(parse_success, "parse error");
    auto inputCount = ptr->num_inputs;
    assert(inputCount > 0, "inputCount");
    return ptr;
}

void runForInput(parsed_input* ptr);

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
    assert(read >= 0, "pipe-create-read");
    assert(write >= 0, "pipe-create-write");
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

// Duplicates the file descriptor, old and new file descriptors can be used interchangeably.
void self_dup2(int a, int b){
    auto result = dup2(a, b);
    assert(result >= 0 , "dup error");
}

void redirect(int fd1, int fd2){
    self_dup2(fd1, fd2);
    closeFile(fd1);
}

void redirectInput(int readFd, int currentInFd){
    redirect(readFd, currentInFd);
}

void redirectOutput(int writeFd, int currentOutFd){
    redirect(writeFd, currentOutFd);
}

// After the call to this, writing to stdout goes to writefd. Writefd can be deleted.
void redirectStdout(int writeFd){
    redirectOutput(writeFd, STDOUT_FILENO);
}

// After the call to this, stdin reads what's been written to readfd. Readfd can be deleted.
void redirectStdin(int readFd){
    redirectInput(readFd, STDIN_FILENO);
}

void runCommand(char* args[]){
    execvp(args[0], args);

    // Execvp shouldn't return
    assert(false, "execvp error");
}

void copyString(char*& src, char*& dst){
    if(src == NULL){
        dst = NULL;
        return;
    }

    auto srcLength = strlen(src) + 1;
    dst = new char[srcLength];
    memcpy(dst, src, srcLength);
}

void copyInPlace(char* src, char* dst, int count){
    memcpy(dst, src, count);
}

void getCommand(single_input& input, CommandSubshellArgs& result){
    auto type = input.type;

    if(type == INPUT_TYPE_COMMAND){
        result.isCommand = true;
        for(int x = 0; x < MAX_ARGS; x++){
                auto& src = input.data.cmd.args[x];
                auto& dst = result.commandArgs.args[x];
                copyString(src, dst);
            }
    } else if(type == INPUT_TYPE_SUBSHELL){
        result.isCommand = false;
        copyInPlace(input.data.subshell, result.subshellArgs.str, INPUT_BUFFER_SIZE);
    } else{
        assert(false, "getpipelineargs-2");
    }
}

PipelineArgs getPipeline(pipeline& pipeline){
    PipelineArgs result;
    result.count = pipeline.num_commands;

    for(int i = 0; i < result.count; i++){
        single_input src;
        src.type = INPUT_TYPE_COMMAND;

        for(int x = 0; x < MAX_ARGS; x++){
            copyString(pipeline.commands[i].args[x], src.data.cmd.args[x]);
        }

        getCommand(src, result.commands[i]);
    }

    return result;
}

PipelineArgs getPipeline(parsed_input* parsed_input){
    assert(parsed_input->separator == SEPARATOR_PIPE, "getpipelineargs-1");
    auto count = (int)parsed_input->num_inputs;
    PipelineArgs result;
    result.count = count;

    for(int i = 0; i < count; i++){
        auto input = parsed_input->inputs[i];
        getCommand(input, result.commands[i]);
    }

    return result;
}

void writeToPipe(int writeFd, string& line){
    auto charPtr = const_cast<char*>(line.c_str());
    auto count = (int)line.size();
    auto result = write(writeFd, charPtr, count);
    char newline[1];
    newline[0] = '\n'; 
    write(writeFd, newline, 1);

    if(result < 0){
        fprintf(stderr, "Write failed: %s\n", strerror(errno));
        assert(false, "pipe-write");
    }
}

bool is_closed(int fd) {
    return fcntl(fd, F_GETFL) == -1;
}

void runRepeater(parsed_input* input){
    assert(input->separator == SEPARATOR_PARA, "repeater");

    // cout << "RunRepeater" << endl;
    // We're already forked and piped (previous process is sending input to us)

    auto inputCount = input->num_inputs;
    auto pipeReadFds = new int[inputCount];
    auto pipeWriteFds = new int[inputCount];

    for(int i = 0; i < inputCount; i++){
        pipe(pipeReadFds[i], pipeWriteFds[i]);
    }

    vector<pid_t> childPids;
    vector<string> lines;
    string line;
    while(getline(cin, line)){
        lines.push_back(line);
    }

    auto lineCount = (int)lines.size();

    for(int i = 0; i < inputCount; i++){
        auto type = input->inputs[i].type;
        assert(type == INPUT_TYPE_COMMAND, "repeater-2");
        auto args = input->inputs[i].data.cmd.args;

        pipe(pipeReadFds[i], pipeWriteFds[i]);

        bool isChild;
        pid_t childPid;
        fork(isChild, childPid);

        // Rep->A, Rep->B, Rep->C
        if(isChild){
            
            // A, B, C, Receives from Repeater
            redirectStdin(pipeReadFds[i]);

            // cout << "RunningOnChild" << endl;

            for(int x = i; x >= 0; x--){
                // It works if I leave it like this, no idea why it works though...
                closeFile(pipeWriteFds[x]);
            }

            // cout << "BeforeRunningChildProgram" << endl;

            runCommand(args);
        } else{
            // Repeater program
            childPids.push_back(childPid);
        }
    }

    for(int i = 0; i < inputCount; i++){
        assert(!is_closed(pipeReadFds[i]), "read-end closed");
        assert(!is_closed(pipeWriteFds[i]), "write-end closed");
    }

    for(int i = 0; i < inputCount; i++){
        for(int j = 0; j < lineCount; j++){
            auto& line = lines[j];
            // cout << "pipeValue is " << pipeWriteFds[i] << endl ;
            // cout << "line value is " << line << endl;
            writeToPipe(pipeWriteFds[i], line);
        }

        // cout << "sent input to number " << i << endl;
    }

    // Close files for eof
    for(int i = 0; i < inputCount; i++){
        closeFile(pipeWriteFds[i]);
    }

    auto childCount = (int)childPids.size();
    for(int i = 0; i < childCount; i++){
        // cout << "Waiting for child..." << childPids[i] << endl;
        waitForChildProcess(childPids[i]);
        // cout << "One child process exited: " << childPids[i] << endl;
    }

    delete[] pipeReadFds;
    delete[] pipeWriteFds;
}

// We alreayd know we're in the pipeline here
void runPipeline(const PipelineArgs& input)
{
    // cout << "RunPipelineStarted" << endl;

    vector<pid_t> childPids;
    auto inputCount = (int)input.count;

    int pipeCount = inputCount - 1;
    int* pipeWriteFds = new int[pipeCount];
    int* pipeReadFds = new int[pipeCount];

    // Example: A | B | C
    // F1: OG/A, F2: OG/B, F3: OG/C (Requires 3 fork)
    // P1: A->B, P2: B->C (Requires 2 pipe)
    for (int i = 0; i < inputCount; i++)
    {
        auto currentCommand = input.commands[i];
  
        if(i != inputCount - 1){
            // cout << "Create pipe at index" << i << endl;
            pipe(pipeReadFds[i], pipeWriteFds[i]);
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
                
                redirectStdin(pipeReadFds[i - 1]);
            }

            // For any process, we need to close the write-ends that's been created so far,
            // Otherwise the child process can't detect EOF
            for(int x = i - 1; x >= 0; x--){
                closeFile(pipeWriteFds[x]);
            }

            // A writes to B
            // B writes to C
            if(i != inputCount - 1){
                // cout << "redirect stdout: " << i << endl;
                redirectStdout(pipeWriteFds[i]);
            }

            if(currentCommand.isCommand){
                // Notice program a doesn't continue after here
                runCommand(currentCommand.commandArgs.args);
            } else {
                
                char* str = currentCommand.subshellArgs.str;
                auto input = parseInput(str);
                auto isParallel = input->num_inputs > 1 && input->separator == SEPARATOR_PARA;

                if(isParallel){
                    runRepeater(input);
                    exit(0);
                } else{
                    runForInput(input);
                    exit(0);
                }
            }
        } else{
            // cout << "Create Child: " << childPid << endl;
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
        // cout << "Waiting for child..." << childPids[i] << endl;
        waitForChildProcess(childPids[i]);
        // cout << "One child process exited: " << childPids[i] << endl;
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
                runCommand(args);
            } else if(type == INPUT_TYPE_PIPELINE){
                runPipeline(getPipeline(input->inputs[i].data.pline));
                exit(0);
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

    // cout << "Parallel Run Done." << endl;
}

void runSequential(parsed_input* input){
    // Example: A ; B ; C
    // Forking: OG/A, OG/B, OG/C (3 times)
    auto inputCount = (int)input->num_inputs;
    assert(inputCount > 1, "numinputs");

    // cout << "Sequential Run started." << endl;

    for(int i = 0; i < inputCount; i++){
        bool isChild;
        pid_t childPid;
        fork(isChild, childPid);

        if(isChild){
            auto type = input->inputs[i].type;
            if(type == INPUT_TYPE_COMMAND){
                auto args = input->inputs[i].data.cmd.args;
                runCommand(args);
            } else if(type == INPUT_TYPE_PIPELINE){
                // Notice: It runs the pipeline as the main program, we need to kill it.
                runPipeline(getPipeline(input->inputs[i].data.pline));
                exit(0);
            } else{
                assert(false, "inputtype-seq");
            }
        } else{
            waitForChildProcess(childPid);
        }
    }

    // cout << "Sequential Run Done!" << endl;
}

void runSingleSubshell(char* str){
    bool isChild;
    pid_t childPid;
    fork(isChild, childPid);

    if(isChild){
        runForInput(parseInput(str));
        exit(0);
    } else{
        waitForChildProcess(childPid);
    }
}

void runSingleCommand(parsed_input* input){
    auto type = input->inputs[0].type;
    bool isChild;
    pid_t childPid;
    fork(isChild, childPid);

    assert(type == INPUT_TYPE_COMMAND, "inputtype-singlecommand");

    if(isChild){
        auto args = input->inputs[0].data.cmd.args;
        // Child process inherits the stdout from parent, no need for redirection
        runCommand(args);
    } else{
        waitForChildProcess(childPid);
    }
}

void runNoSeparator(parsed_input* input){
    auto type = input->inputs[0].type;
    if(type == INPUT_TYPE_COMMAND){
        runSingleCommand(input);
    } else if(type == INPUT_TYPE_SUBSHELL){
        auto& subshell = input->inputs[0].data.subshell;
        runSingleSubshell(subshell);
        // runForInput(parseInput(subshell));
    } else{
        assert(false, "unexpected input no separator");
    }
}

void runForInput(parsed_input* ptr){
    // pretty_print(ptr);
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
        runPipeline(getPipeline(ptr));
        break;
    }
    case SEPARATOR_SEQ:
    {
        runSequential(ptr);
        break;
    }
    case SEPARATOR_NONE:
    {
        runNoSeparator(ptr);
        break;
    }
    default:
        {
            cout << "UNEXPECTED SEPARATOR" << endl;
            exit(-1);
        }
    }

    free_parsed_input(ptr);
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

        // cout << "Running For Input: '" << inputLine << "'" << endl;

        auto cPtr = const_cast<char *>(inputLine.c_str());
        runForInput(parseInput(cPtr));
        // cout << "Expecting Input." << endl;

        cout << "/> ";
        getline(cin, inputLine);

        // cout << "getline received next input: " << inputLine << endl;
    }

    // cout << "quitting..." << endl;
    return 0;
}