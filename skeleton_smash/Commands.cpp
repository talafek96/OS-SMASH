#include <unistd.h>
#include <string.h>
#include <iostream>
#include <fcntl.h>
#include <vector>
#include <sstream>
#include <sys/wait.h>
#include <iomanip>
#include "Commands.h"
#include "Exceptions.h"

using namespace std;

#if 0
#define FUNC_ENTRY() \
    cout << __PRETTY_FUNCTION__ << " --> " << endl;

#define FUNC_EXIT() \
    cout << __PRETTY_FUNCTION__ << " <-- " << endl;
#else
#define FUNC_ENTRY()
#define FUNC_EXIT()
#endif

#define READ_BUFFER_SIZE 1024

enum class CMD_Type
{
    Normal, Background, Pipe, ErrPipe,
    OutRed, OutAppend
};

const std::string WHITESPACE = " \n\r\t\f\v";

string _ltrim(const std::string &s)
{
    size_t start = s.find_first_not_of(WHITESPACE);
    return (start == std::string::npos) ? "" : s.substr(start);
}

string _rtrim(const std::string &s)
{
    size_t end = s.find_last_not_of(WHITESPACE);
    return (end == std::string::npos) ? "" : s.substr(0, end + 1);
}

string _trim(const std::string &s)
{
    return _rtrim(_ltrim(s));
}

int _parseCommandLine(const char *cmd_line, char **args)
{
    FUNC_ENTRY()
    int i = 0;
    std::istringstream iss(_trim(string(cmd_line)).c_str());
    for (std::string s; iss >> s;)
    {
        args[i] = (char *)malloc(s.length() + 1);
        memset(args[i], 0, s.length() + 1);
        strcpy(args[i], s.c_str());
        args[++i] = NULL;
    }
    return i; 

    FUNC_EXIT()
} 

// Processes the command line, estimates the type of the command and returns it.
// Fills the args array with malloc'ed C type strings of the arguments, after seperating the operators.
// If args_count is given, put the number of total arguments inside of it (including a NULL argument).
CMD_Type _processCommandLine(const char* cmd_line, char** args = nullptr, int* args_count = nullptr)
{
    CMD_Type res = CMD_Type::Normal;
    std::string cmd(cmd_line);
    std::size_t pos = cmd.find_first_of(">|"); 
    if(pos != std::string::npos)
    {
        char oper[2];
        char half1[COMMAND_ARGS_MAX_LENGTH], half2[COMMAND_ARGS_MAX_LENGTH];
        oper[1] = 0;
        int n1, n2;
        switch(cmd[pos])
        {
            case '>':
            {
                oper[0] = '>';
                if(cmd[pos+1] == '>')
                {
                    oper[1] = '>';
                    res = CMD_Type::OutAppend;
                    goto DoubleOp;
                }
                else
                {
                    res = CMD_Type::OutRed;
                    goto SingleOp;
                }
                break;
            }
            case '|':
            {
                oper[0] = '|';
                if(cmd[pos+1] == '&')
                {
                    oper[1] = '&';
                    res = CMD_Type::ErrPipe;
                    goto DoubleOp;
                }
                else
                {
                    res = CMD_Type::Pipe;
                    goto SingleOp;
                }
                break;
            }
            default: // Should not get here
                break;
        }

    DoubleOp:
        if(args)
        {
            for(int i = 0; i < pos; i++)
            {
                half1[i] = cmd_line[i];
            }
            half1[pos] = 0; // Copy first half command
            strcpy(half2, cmd_line + pos + 2); // Copy second half command
            n1 = _parseCommandLine(half1, args); // Load first half to args arr
            args[n1] = (char*)malloc(3 * sizeof(char)); 
            *args[n1] = oper[0]; 
            *(args[n1] + 1) = oper[1]; 
            *(args[n1] + 2) = 0; // Load two operators to the next args block
            n2 = _parseCommandLine(half2, args + n1 + 1); // Load second half to args arr
        }
        goto End;

    SingleOp:
        if(args)
        {
            for(int i = 0; i < pos; i++)
            {
                half1[i] = cmd_line[i];
            }
            half1[pos] = 0; // Copy first half command
            strcpy(half2, cmd_line + pos + 1); // Copy second half command
            n1 = _parseCommandLine(half1, args); // Load first half to args arr
            args[n1] = (char*)malloc(2 * sizeof(char));
            *args[n1] = oper[0];
            *(args[n1] + 1) = 0; // Load two operators to the next args block
            n2 = _parseCommandLine(half2, args + n1 + 1); // Load second half to args arr
        }
        goto End;

    End:
        if(args_count && args)
        {
            *args_count = n1 + n2 + 1;
        }
        // Dragons be here
    }
    else // No special operator
    {
        if(_isBackgroundCommand(cmd_line)) // Insert the args after removing the ampersand.
        {
            char copy[COMMAND_ARGS_MAX_LENGTH];
            strcpy(copy, cmd_line);
            _removeBackgroundSign(copy);
            if(args)
            {
                int n = _parseCommandLine(copy, args);
                if(args_count) 
                {
                    *args_count = n;
                }
            }
            res = CMD_Type::Background;
        }
        else
        {
            if(args)
            {
                int n = _parseCommandLine(cmd_line, args);
                if(args_count) 
                {
                    *args_count = n;
                }
            }
        }
    }
    return res;
}

bool _isBackgroundCommand(const char *cmd_line)
{
    const string str(cmd_line);
    return str[str.find_last_not_of(WHITESPACE)] == '&';
}

void _removeBackgroundSign(char *cmd_line)
{
    const string str(cmd_line);
    // find last character other than spaces
    unsigned int idx = str.find_last_not_of(WHITESPACE);
    // if all characters are spaces then return
    if (idx == string::npos)
    {
        return;
    }
    // if the command line does not end with & then return
    if (cmd_line[idx] != '&')
    {
        return;
    }
    // replace the & (background sign) with space and then remove all tailing spaces.
    cmd_line[idx] = ' ';
    // truncate the command line string up to the last non-space character
    cmd_line[str.find_last_not_of(WHITESPACE, idx) + 1] = 0;
}

// TODO: Add your implementation for classes in Commands.h

bool SmallShell::isPwdSet() const
{
    return last_pwd.empty();
}

void SmallShell::setLastPwd(const std::string& new_pwd)
{
    last_pwd = new_pwd;
}

const std::string& SmallShell::getLastPwd() const
{
    return last_pwd;
}

/**
* Creates and returns a pointer to Command class which matches the given command line (cmd_line)
*/
Command* SmallShell::CreateCommand(const char *cmd_line)
{
    char *args[COMMAND_MAX_ARGS];
    int args_num;
    CMD_Type type = _processCommandLine(cmd_line, args, &args_num);
    switch(type)
    {
        case CMD_Type::Normal:
        {
            std::string first_arg(args[0]);
            if(!first_arg.compare("chprompt"))
            {
                arrayFree(args, args_num);
                return new ChpromptCommand(cmd_line);
            }
            else if(!first_arg.compare("quit"))
            {
                arrayFree(args, args_num);
                return new QuitCommand(cmd_line, SmallShell::getInstance().getJobsList());
            }
            else if(!first_arg.compare("showpid"))
            {
                arrayFree(args, args_num);
                return new ShowPidCommand(cmd_line);
            }
            else if(!first_arg.compare("pwd"))
            {
                
                arrayFree(args, args_num);
                return new GetCurrDirCommand(cmd_line);
            }
            else if(!first_arg.compare("jobs"))
            {
                arrayFree(args, args_num);
                return new JobsCommand(cmd_line, SmallShell::getInstance().getJobsList());
            }
            else if(!first_arg.compare("kill"))
            {
                if(!args[1] || !args[2] || args[3] \
                || args[1][1] == '\0' || !isNumber(std::string(args[2]))) // Check correctness of the arguments
                {
                    arrayFree(args, args_num);
                    throw InvalidArgs("kill");
                }
                int signum = 0, job_id = 0;
                bool res = extractIntFlag(args[1], &signum); // Check and extract the flag arg
                if(!res || !isNumber(static_cast<std::string>(args[2])))
                {
                    arrayFree(args, args_num);
                    throw InvalidArgs("kill");
                }
                std::istringstream arg3(args[2]);
                arg3 >> job_id; // Extract the job id
                if(SmallShell::getInstance().getJobsList()->getJobById(job_id) == nullptr) // Check if the job currently exists
                {
                    arrayFree(args, args_num);
                    throw JobDoesNotExist("kill", job_id);
                }
                arrayFree(args, args_num);
                return new KillCommand(cmd_line, signum, job_id, SmallShell::getInstance().getJobsList());
            }
            else if(!first_arg.compare("cd"))
            {
                if (args_num > 2)
                {
                    arrayFree(args, args_num);
                    throw TooManyArgs("cd");
                }
                else if (args_num==2)
                {
                    if (SmallShell::getInstance().getLastPwd().empty() && (!static_cast<std::string>("-").compare(args[1])))
                    {
                        arrayFree(args, args_num);
                        throw OldPwdNotSet("cd");
                    }
                    
                    arrayFree(args, args_num);
                    return new ChangeDirCommand(cmd_line);
                }
                else
                {
                    return nullptr;
                }
            }
            else if(!first_arg.compare("cat"))
            {
                if(args_num < 2)
                {
                    arrayFree(args, args_num);
                    throw NotEnoughArgs("cat");
                }
                arrayFree(args, args_num);
                return new CatCommand(cmd_line);
            }

            else if(!first_arg.compare("fg"))
            {
                if (args_num > 1)
                {
                    int job_id;
                    std::istringstream arg2(args[1]);
                    
                    if(!isNumber(args[1], true) || args_num > 2)
                    {
                        arrayFree(args, args_num);
                        throw InvalidArgs("fg");
                    }
                    arg2 >> job_id;

                    if(!SmallShell::getInstance().getJobsList()->getJobById(job_id))
                    {
                        arrayFree(args, args_num);
                        throw JobDoesNotExist("fg", job_id);
                    }
                }

                if (args_num==1 && SmallShell::getInstance().getJobsList()->isEmpty())
                {
                    arrayFree(args, args_num);
                    throw JobsListIsEmpty("fg");
                }
                arrayFree(args, args_num);
                return new ForegroundCommand(cmd_line);
            }
            else if(!first_arg.compare("bg"))
            {
                if (args_num > 1)
                {
                    int job_id;
                    std::istringstream arg2(args[1]);
                    
                    if (!isNumber(args[1], true) || args_num > 2)
                    {
                        arrayFree(args, args_num);
                        throw InvalidArgs("bg");
                    }
                    arg2 >> job_id;

                    if (!SmallShell::getInstance().getJobsList()->getJobById(job_id))
                    {
                        arrayFree(args, args_num);
                        throw JobDoesNotExist("bg", job_id);
                    }

                    // If job is not stopped then it's in background (you cannot type this command if its in the foreground)
                    if (SmallShell::getInstance().getJobsList()->getJobById(job_id)->state != STOPPED)
                    {
                        arrayFree(args, args_num);
                        throw JobIsAlreadyBackground("bg", job_id);
                    }
                }

                if(args_num == 1 && (!SmallShell::getInstance().getJobsList()->getLastStoppedJob()))
                {
                    arrayFree(args, args_num);
                    throw NoStoppedJob("bg");
                }
                arrayFree(args, args_num);
                return new BackgroundCommand(cmd_line);
            }

            else if(args_num)
            {
                arrayFree(args, args_num);
                return new ExternalCommand(cmd_line, false);
            }
            break;
        }
        case CMD_Type::Background:
        {
            if(args_num)
            {
                arrayFree(args, args_num);
                return new ExternalCommand(cmd_line, true);
            }
            
            break;
        }
        default: // Should not get here.
            break;
    }
    arrayFree(args, args_num);
    return nullptr;
} 

void SmallShell::executeCommand(const char *cmd_line)
{
    SmallShell::getInstance().getJobsList()->updateAllJobs(); // Update all the jobs upon execution
    CMD_Type type = _processCommandLine(cmd_line);
    switch(type)
    {
        case CMD_Type::Normal:
        {
            Command* cmd = nullptr;
            cmd = CreateCommand(cmd_line);
            if(cmd != nullptr)
            {
                try
                {
                    cmd->execute();
                }
                catch(const std::exception& e)
                {
                    std::cerr << e.what() << '\n';
                }
            }
            break;
        }
        case CMD_Type::Background:
        {
            if(!isBuiltIn(cmd_line))
            {
                Command* cmd;
                try
                {
                    cmd = CreateCommand(cmd_line);
                }
                catch(const std::exception& e)
                {
                    std::cerr << e.what() << '\n';
                }
                if(cmd != nullptr)
                {
                    try
                    {
                        cmd->execute();
                    }
                    catch(const std::exception& e)
                    {
                        std::cerr << e.what() << '\n';
                    }
                }
            }
            break;
        }
        default: // Should not get here
            break;
    }
}

//*********************AUXILIARY********************//
void arrayFree(char **arr, int len)
{
    for(int i = 0; i < len; i++)
    {
        free(arr[i]);
    }
}

bool isNumber(const std::string& str, bool is_unsigned)
{
    int i = 0;
    for(char const& c : str) 
    {
        if(!is_unsigned)
        {
            if(i++ == 0)
            {
                if(c == '-')
                {
                    continue;
                }
            }
        }
        
        if(!std::isdigit(c)) 
        {
            return false;
        }
    }
    return true;
}

/**
 * Extracts the flag from the given string if it is a valid flag format.
 * Returns true if it is a valid flag and puts the flag in flag_num (if no NULL),
 * Otherwise returns false.
 */
bool extractIntFlag(const std::string& str, int* flag_num)
{
    std::string s1(_trim(str));
    std::istringstream ss(s1.c_str());
    char c = ss.get();
    if(c != '-' || !isNumber(s1.substr(1)))
    {
        return false;
    }
    if(flag_num)
    {
        ss >> *flag_num;
    }
    return true;
}

//******************COMMAND CLASSES*****************//
const std::string& Command::getCmdLine() const
{
    return cmd_text;
}

int Command::getPid() const
{
    return pid;
}

bool Command::isBackground() const
{
    return is_background;
}

ChpromptCommand::ChpromptCommand(const char *cmd_line) : BuiltInCommand(cmd_line)
{
    char *args[COMMAND_MAX_ARGS];
    int n;
    _processCommandLine(cmd_line, args, &n);
    if(args[1] != NULL)
    {
        new_prompt = args[1];
    }
    else
    {
        new_prompt = "smash";
    }
    arrayFree(args, n);
}

void ChpromptCommand::execute() // chprompt exec
{
    SmallShell::getInstance().setPrompt(new_prompt);
}

QuitCommand::QuitCommand(const char *cmd_line, std::shared_ptr<JobsList> jobs) : BuiltInCommand(cmd_line), jobs(jobs)
{
    char *args[COMMAND_MAX_ARGS];
    int n;
    _processCommandLine(cmd_line, args, &n);
    if (n > 1) //there is another argument after quit
    {
        if(!static_cast<std::string>("kill").compare(args[1]))
        {
            is_kill = true;
        }
    }
    arrayFree(args, n);
}


void QuitCommand::execute()
{
    if(is_kill)
    {
        jobs->killAllJobs();
    }
    SmallShell::getInstance().quit_flag = true;
}

ShowPidCommand::ShowPidCommand(const char *cmd_line) : BuiltInCommand(cmd_line) { }

void ShowPidCommand::execute()
{
    std::cout << "smash pid is " << getpid() << " " << std::endl;
}

GetCurrDirCommand::GetCurrDirCommand(const char* cmd_line) : BuiltInCommand(cmd_line) { }

void GetCurrDirCommand::execute()
{
    char buff[COMMAND_ARGS_MAX_LENGTH];
    if(getcwd(buff, COMMAND_ARGS_MAX_LENGTH) == NULL)
    {
        std::cerr << SyscallError("getcwd").what() << std::endl;
    }
    else
    {
        std::cout << buff << std::endl;
    }
}

ChangeDirCommand::ChangeDirCommand(const char *cmd_line): BuiltInCommand(cmd_line)
{
    char *args[COMMAND_MAX_ARGS];
    int n;
    _processCommandLine(cmd_line, args, &n);
    pathname = args[1];
    arrayFree(args, n);
}

void ChangeDirCommand::execute()
{
    char current[COMMAND_ARGS_MAX_LENGTH];
    if (!static_cast<std::string>("-").compare(pathname))
    {
        if(getcwd(current, COMMAND_ARGS_MAX_LENGTH) == NULL)
        {
            throw SyscallError("getcwd");
        }
        if (chdir((SmallShell::getInstance().getLastPwd()).c_str()) == -1) // Attempt to change the dir to the previous one
        {
            throw SyscallError("chdir");
        }
        // On success, replace the previous dir with the current one (on the smash memory)
        
        SmallShell::getInstance().setLastPwd(current);
    }
    else
    {
        if(getcwd(current, COMMAND_ARGS_MAX_LENGTH) == NULL)
        {
            throw SyscallError("getcwd");
        }
        if ((chdir(pathname.c_str()))==-1)
        {
            throw SyscallError("chdir");
        }
        SmallShell::getInstance().setLastPwd(current);
    }
}

JobsCommand::JobsCommand(const char *cmd_line, std::shared_ptr<JobsList> jobs) : BuiltInCommand(cmd_line), jobs(jobs) { }

void JobsCommand::execute()
{
    jobs->printJobsList();
}

KillCommand::KillCommand(const char *cmd_line, int signum, int job_id, std::shared_ptr<JobsList> jobs) :
BuiltInCommand(cmd_line), signum(signum), job_id(job_id), jobs(jobs) { }

void KillCommand::execute()
{
    if(kill(jobs->getJobById(job_id)->pid, signum) == -1)
    {
        perror("smash error: kill failed");
    }
}

ExternalCommand::ExternalCommand(const char* cmd_line, bool is_background) : 
Command(cmd_line, is_background, pid), is_background(is_background), stripped_cmd("")
{ 
    if(is_background)
    {
        char* tmp;
        if(!(tmp = strdup(cmd_line)))
        {
            throw std::bad_alloc();
        }
        _removeBackgroundSign(tmp);
        stripped_cmd = std::string(tmp);
        free(tmp);
    }
}

void ExternalCommand::execute()
{
    char args_line[COMMAND_ARGS_MAX_LENGTH], bash[] = "/bin/bash", c_flag[] = "-c";
    pid_t c_pid;

    if((c_pid = fork()) == -1)
    {
        throw SyscallError("fork");
    }
    else if(c_pid == 0) // Child
    {
        setpgrp();
        strcpy(args_line, is_background? stripped_cmd.c_str() : cmd_text.c_str());
        char *exec_args[] = {bash, c_flag, args_line, NULL};
        if(execvp("/bin/bash", exec_args) == -1)
        {
            throw SyscallError("execvp");
        }
    }
    else 
    {
        this->pid = c_pid;
        SmallShell::getInstance().getJobsList()->addJob(this);
        if(!is_background)
        {
            if(waitpid(pid, NULL, WUNTRACED) == -1)
            {
                throw SyscallError("waitpid");
            }
        }
    }
}

CatCommand::CatCommand(const char *cmd_line) : BuiltInCommand(cmd_line)
{
    char *args[COMMAND_MAX_ARGS];
    int n;
    _processCommandLine(cmd_line, args, &n);
    for(int i = 1; i < n; i++)
    {
        if(args[i] != NULL)
        {
            f_queue.emplace(args[i]);
        }
    }
    arrayFree(args, n);
}

void CatCommand::execute()
{
    std::string curr_file;
    char buffer[READ_BUFFER_SIZE];
    int fd = 0, read_res = 1;

    while(!f_queue.empty()) // Repeat until the queue is empty
    {
        curr_file = f_queue.front();
        f_queue.pop();
        read_res = 1; // Might be superfluous.

        if((fd = open(curr_file.c_str(), O_RDONLY)) == -1)
        {
            throw SyscallError("open");
        }
        while((read_res = read(fd, buffer, READ_BUFFER_SIZE)))
        {
            if(read_res == -1)
            {
                throw SyscallError("read");
            }
            if(write(STDOUT_FILENO, buffer, read_res) == -1)
            {
                throw SyscallError("write");
            }
        }
    }
}


ForegroundCommand::ForegroundCommand(const char *cmd_line) : BuiltInCommand(cmd_line)
{
    char *args[COMMAND_MAX_ARGS];
    int n;
    _processCommandLine(cmd_line, args, &n);
    if(n > 1)
    {
        std::istringstream arg2(args[1]);
        arg2 >> job_id_to_fg;
    }
    arrayFree(args, n);
}

void ForegroundCommand::execute()
{
    int job_id;
    if (job_id_to_fg) //if we got job id in the input command line
    {
        job_id = job_id_to_fg;
    }
    else
    {
        SmallShell::getInstance().getJobsList()->getLastJob(&job_id);
    }
    const std::shared_ptr<JobEntry>& jcb = SmallShell::getInstance().getJobsList()->getJobById(job_id);
    std::cout << jcb->command << " : " << jcb->pid << std::endl;
    
    SmallShell::getInstance().setCurrentFg(job_id);
    if(jcb->state==STOPPED)
    {
        if (kill(jcb->pid, SIGCONT) == -1)
        {
            throw SyscallError("kill");
        }
        jcb->state==RUNNING;
    }
    jcb->is_background = false;
    if(waitpid(jcb->pid, NULL, WUNTRACED) == -1)
    {
        throw SyscallError("waitpid");
    }
}

BackgroundCommand::BackgroundCommand(const char *cmd_line) : BuiltInCommand(cmd_line)
{
    char *args[COMMAND_MAX_ARGS];
    int n;
    _processCommandLine(cmd_line, args, &n);
    if(n > 1)
    {
        std::istringstream arg2(args[1]);
        arg2 >> job_id_to_bg;
    }
    arrayFree(args, n);
}

void BackgroundCommand::execute()
{
    int job_id, status;
    if(job_id_to_bg) //if we got job id in the input command line
    {
        job_id = job_id_to_bg;
    }
    else
    {
        // re-check just in case a sigcont was sent asynchroniously
        if(!SmallShell::getInstance().getJobsList()->getLastStoppedJob(&job_id)) 
        {
            throw NoStoppedJob("bg");
        }
    }
    const std::shared_ptr<JobEntry>& jcb = SmallShell::getInstance().getJobsList()->getJobById(job_id);
    std::cout << jcb->command << " : " << jcb->pid << std::endl;
    if(kill(jcb->pid, SIGCONT) == -1)
    {
        throw SyscallError("kill");
    }
    jcb->is_background = true;
    jcb->state = RUNNING;
}


//***************SMASH IMPLEMENTATION***************//
SmallShell::~SmallShell()
{
    // TODO: add your implementation
}

const std::string& SmallShell::getPrompt() const
{
    return prompt;
}

void SmallShell::setPrompt(const std::string& new_prompt)
{
    prompt = new_prompt;
}

bool SmallShell::isBuiltIn(const char* cmd_line) const
{
    string cmd_s = _trim(string(cmd_line));
    string firstWord = cmd_s.substr(0, cmd_s.find_first_of(" \n"));
    if(builtin_set.count(firstWord))
    {
        return true;
    }
    return false;
}

bool SmallShell::getQuitFlag() const
{
    return quit_flag;
}

std::shared_ptr<JobsList> SmallShell::getJobsList()
{
    return jobs;
}

int SmallShell::getCurrentFg() const
{
    return fg_job_id;
}

void SmallShell::setCurrentFg(int job_id)
{
    fg_job_id = job_id;
}

//*************JOBSLIST IMPLEMENTATION*************//
void JobsList::addJob(Command *cmd, bool isStopped)
{
    /*
        struct JobEntry
        {
            int job_id;
            int pid;
            std::string command;
            time_t start_time;
            j_state state;
            bool background;
        };
    */
    int job_id;
    if(jobs.size() == 0) // In case there are no current jobs running.
    {
        job_id = 1;
    }
    else // Otherwise: max job_id + 1
    {
        job_id = jobs.rbegin()->first + 1;
    }
    int pid = cmd->getPid();
    std::string command = cmd->getCmdLine();
    time_t start_time = time(NULL);
    j_state state = isStopped? j_state::STOPPED : j_state::RUNNING;
    bool is_background = cmd->isBackground();
    JobEntry jcb = {job_id, pid, command, start_time, state, is_background}; // Create the JCB template.
    jobs[job_id] = std::make_shared<JobEntry>(jcb); // Add the new job to the jobs map. (AS A SHARED_PTR)
    if(!is_background && !isStopped)
    {
        SmallShell::getInstance().fg_job_id = job_id;
    }
}

void JobsList::updateAllJobs()
{
    int status;
    std::list<int> erase_list;
    SmallShell& smash = SmallShell::getInstance();
    for(auto& pair : jobs)
    {
        std::shared_ptr<JobEntry>& jcb = pair.second;
        status = 0;
        if(waitpid(jcb->pid, &status, WNOHANG) != 0) // If the job is dead, remove it.
        {
            if(WIFSTOPPED(status)) // If job is only stopped, update it's status.
            {
                jcb->state = j_state::STOPPED;
            }
            erase_list.push_front(jcb->job_id);
            if(jcb->job_id == smash.fg_job_id)
            {
                smash.fg_job_id = 0;
            }
        }
    }

    for(auto& pair : jobs)
    {
        std::shared_ptr<JobEntry>& jcb = pair.second;
        status = 0;
        if(waitpid(jcb->pid, &status, WUNTRACED | WNOWAIT | WNOHANG) > 0) // If the job is stopped, update it.
        {
            if(WIFSTOPPED(status)) // If job is only stopped, update it's status.
            {
                if(jcb->state == j_state::RUNNING)
                {
                    jcb->start_time = time(NULL);
                }
                jcb->state = j_state::STOPPED;
            }
        }
    }

    // Actually perform the deletion:
    for(auto& job_id : erase_list)
    {
        jobs.erase(job_id);
    }
}

void JobsList::printJobsList()
{
    updateAllJobs();
    auto now = time(NULL);
    for(auto& pair : jobs)
    {
        std::shared_ptr<JobEntry>& jcb = pair.second;
        std::cout << "[" << jcb->job_id << "]" << jcb->command << " : " << jcb->pid << " " << difftime(now, jcb->start_time) << " secs" \
            << ((jcb->state == j_state::STOPPED)? " (stopped)\n" : "\n");
    }
}

void JobsList::killAllJobs(bool print)
{
    updateAllJobs();
    std::list<int> erase_list;
    if(print)
    {
        std::cout << "smash: sending SIGKILL signal to " << jobs.size() << " jobs:\n";
    }
    for(auto& pair : jobs)
    {
        std::cout << pair.second->pid << ": " << pair.second->command << std::endl;
        if(kill(pair.second->pid, SIGKILL) == -1)
        {
            perror("smash error: kill failed");
        }
    }
    SmallShell::getInstance().fg_job_id = 0;
    jobs.clear();
}

std::shared_ptr<JobEntry> JobsList::getJobById(int jobId)
{
    updateAllJobs();
    if(jobs.count(jobId))
    {
        return jobs[jobId];
        
    }
    return nullptr;
}

bool JobsList::killJobById(int jobId, bool to_update)
{
    if(to_update) updateAllJobs();

    if(jobs.count(jobId))
    {
        std::shared_ptr<JobEntry> jcb = jobs[jobId];

        if(kill(jcb->pid, SIGKILL) == -1)
        {
            perror("smash error: kill failed");
            return false;
        }
        else
        {
            jobs.erase(jcb->job_id); // Erase the job from the jobs map on success
            SmallShell& smash = SmallShell::getInstance();
            if(jobId == smash.fg_job_id)
            {
                smash.fg_job_id = 0;
            }
        }
    }
    return true;
}

std::shared_ptr<JobEntry> JobsList::getLastJob(int *lastJobId = NULL)
{
    updateAllJobs();
    auto last = jobs.rbegin();
    if(last==jobs.rend())
    {
        return nullptr;
    }
    if(lastJobId)
    {
        *lastJobId = last->first;
    }
    return last->second;
}

std::shared_ptr<JobEntry> JobsList::getLastStoppedJob(int *jobId)
{
    updateAllJobs();
    auto last = jobs.rbegin();
    if(last == jobs.rend())
    {
        return nullptr;
    }
    while(last->second->state != j_state::STOPPED) last++;
    if(jobId)
    {
        *jobId = last->first;
    }
    return last->second;
}

bool JobsList::isEmpty() const
{
    return jobs.empty();
}