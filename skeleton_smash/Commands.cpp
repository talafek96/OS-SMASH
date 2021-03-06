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

#define READ_BUFFER_SIZE 4096

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
            for(unsigned i = 0; i < pos; i++)
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
            for(unsigned i = 0; i < pos; i++)
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
Command* SmallShell::CreateCommand(const char *cmd_line, bool valid_job, bool to_wait)
{
    char *args[COMMAND_MAX_ARGS];
    int args_num;
    CMD_Type type = _processCommandLine(cmd_line, args, &args_num);
    std::string first_arg(args[0]);

    switch(type)
    {
        case CMD_Type::Normal:
        {
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
                    
                    if(!isNumber(args[1], false) || args_num > 2)
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
                    
                    if (!isNumber(args[1], false) || args_num > 2)
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

            else if(!first_arg.compare("timeout"))
            {
                if(args_num < 3)
                {
                    arrayFree(args, args_num);
                    throw NotEnoughArgs("timeout");
                }
                if(!isNumber(std::string(args[1]), true))
                {
                    arrayFree(args, args_num);
                    throw InvalidArgs("timeout");
                }
                std::istringstream time_text(args[1]);
                int duration = 0;
                time_text >> duration;

                if(duration == 0)
                {
                    arrayFree(args, args_num);
                    throw InvalidArgs("timeout");
                }

                arrayFree(args, args_num);
                return new TimeoutCommand(cmd_line, false, duration, valid_job);
            }
            else if(args_num) // This is an external command
            {
                arrayFree(args, args_num);
                return new ExternalCommand(cmd_line, false, valid_job, to_wait);
            }
            break;
        }
        case CMD_Type::Background:
        {
            if(!first_arg.compare("timeout"))
            {
                if(args_num < 3)
                {
                    arrayFree(args, args_num);
                    throw NotEnoughArgs("timeout");
                }
                if(!isNumber(std::string(args[1]), true))
                {
                    arrayFree(args, args_num);
                    throw InvalidArgs("timeout");
                }
                std::istringstream time_text(args[1]);
                int duration = 0;
                time_text >> duration;

                if(duration == 0)
                {
                    arrayFree(args, args_num);
                    throw InvalidArgs("timeout");
                }

                arrayFree(args, args_num);
                return new TimeoutCommand(cmd_line, true, duration, valid_job);
            }
            else if(args_num)
            {
                arrayFree(args, args_num);
                return new ExternalCommand(cmd_line, true, valid_job, to_wait);
            }
            break;
        }
        case CMD_Type::OutRed: case CMD_Type::OutAppend:
        {
            if(args_num > 2)
            {
                int op_index = 0;
                bool flag = false;
                for( ; (op_index < args_num) && !flag; op_index++)
                {
                    if(!strcmp(args[op_index], type == CMD_Type::OutRed? ">" : ">>"))
                    {
                        flag = true;
                    }
                }
                op_index--;

                if(op_index != args_num - 2) // The operator must be one before the last word in the args list.
                {
                    arrayFree(args, args_num);
                    return nullptr;
                }
                arrayFree(args, args_num);
                return new RedirectionCommand(cmd_line, type);
            }
            break;
        }
        case CMD_Type::Pipe: case CMD_Type::ErrPipe:
        {
            if(args_num > 2)
            {
                int op_index = 0;
                bool flag = false;
                for( ; (op_index < args_num) && !flag; op_index++)
                {
                    if(!strcmp(args[op_index], type == CMD_Type::Pipe? "|" : "|&"))
                    {
                        flag = true;
                    }
                }
                op_index--;

                if(op_index == 0 || op_index == args_num - 1)
                {
                    arrayFree(args, args_num);
                    return nullptr;
                }

                arrayFree(args, args_num);
                return new PipeCommand(cmd_line, type);
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
        case CMD_Type::OutRed: case CMD_Type::OutAppend: 
        case CMD_Type::Pipe: case CMD_Type::ErrPipe:
        {
            Command* cmd = nullptr;
            cmd = CreateCommand(cmd_line);
            if(cmd != nullptr)
            {
                try
                {
                    cmd->execute();
                    delete cmd;
                }
                catch(const std::exception& e)
                {
                    delete cmd;
                    std::cerr << e.what() << std::endl;
                }
            }
            break;
        }
        case CMD_Type::Background:
        {
            if(!isBuiltIn(cmd_line))
            {
                Command* cmd = nullptr;
                try
                {
                    cmd = CreateCommand(cmd_line);
                }
                catch(const std::exception& e)
                {
                    std::cerr << e.what() << std::endl;
                    if(cmd) delete cmd;
                    return;
                }
                if(cmd != nullptr)
                {
                    try
                    {
                        cmd->execute();
                        delete cmd;
                    }
                    catch(const std::exception& e)
                    {
                        if(cmd) delete cmd;
                        std::cerr << e.what() << std::endl;
                        return;
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
    std::cout << "smash pid is " << SmallShell::getInstance().getPid() << " " << std::endl;
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
    pid_t to_signal = jobs->getJobById(job_id)->pid;
    if(kill(to_signal, signum) == -1)
    {
        perror("smash error: kill failed");
    }
    else
    {
        std::cout << "signal number " << signum << " was sent to pid " << to_signal << std::endl;
    }
}

ExternalCommand::ExternalCommand(const char* cmd_line, bool is_background, bool valid_job, bool to_wait) : 
Command(cmd_line, is_background, pid, valid_job, to_wait), is_background(is_background), stripped_cmd("")
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
        std::shared_ptr<JobEntry> jcb_ptr = nullptr;
        if(this->valid_job) jcb_ptr = SmallShell::getInstance().getJobsList()->addJob(this);

        if(!is_background && to_wait)
        {
            SmallShell::getInstance().setCurrentFg(jcb_ptr == nullptr? 0 : jcb_ptr->job_id, this->pid); // Set the current fg
            if(waitpid(pid, NULL, WUNTRACED) == -1)
            {
                throw SyscallError("waitpid");
            }
            SmallShell::getInstance().setCurrentFg(0, 0); // Reset the current fg
        }
    }
}

TimeoutCommand::TimeoutCommand(const char *cmd_line, bool is_background, int duration, bool valid_job) :
ExternalCommand(cmd_line, is_background, valid_job), duration(duration), command(NULL)
{ 
    std::istringstream ss(cmd_line);
    std::string extracted_cmd;
    ss >> extracted_cmd; ss >> extracted_cmd;
    std::getline(ss, extracted_cmd);
    extracted_cmd = _trim(extracted_cmd);

    command = SmallShell::getInstance().CreateCommand(extracted_cmd.c_str(), false, false);
}

void TimeoutCommand::execute()
{
    command->execute();
    this->pid = command->getPid();
    std::shared_ptr<JobEntry> jcb_ptr = nullptr;
    if(this->valid_job) jcb_ptr = SmallShell::getInstance().getJobsList()->addJob(this);

    SmallShell& smash = SmallShell::getInstance();
    time_t curr_time = time(NULL);
    time_t new_time = curr_time + duration;
    AlarmEntry acb = 
    {
        .finish_time = new_time,
        .pid = this->pid,
        .cmd_text = this->cmd_text
    };
    smash.getAlarmList()->push_front(acb);
    smash.getAlarmList()->sort([](const AlarmEntry &a, const AlarmEntry &b) { return a < b; });
    if(difftime(smash.getAlarmList()->front().finish_time, curr_time) > 0)
    {
        alarm(difftime(smash.getAlarmList()->front().finish_time, curr_time));
    }
    
    if(!is_background && to_wait)
    {
        smash.setCurrentFg(jcb_ptr == nullptr? 0 : jcb_ptr->job_id, this->pid); // Set the current fg
        if(waitpid(pid, NULL, WUNTRACED) == -1)
        {
            throw SyscallError("waitpid");
        }
        smash.setCurrentFg(0, 0); // Reset the current fg.
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
    
    SmallShell::getInstance().setCurrentFg(job_id, jcb->pid);
    if(jcb->state==STOPPED)
    {
        if (kill(jcb->pid, SIGCONT) == -1)
        {
            throw SyscallError("kill");
        }
        jcb->state = RUNNING;
    }
    jcb->is_background = false;
    if(waitpid(jcb->pid, NULL, WUNTRACED) == -1)
    {
        throw SyscallError("waitpid");
    }
    SmallShell::getInstance().setCurrentFg(0, 0);
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
    int job_id;
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

RedirectionCommand::RedirectionCommand(const char *cmd_line, CMD_Type type) : 
Command(cmd_line, false, 0, false), type(type), left_cmd(NULL), stdout_backup(0), write_fd(0), filename("")
{
    // Create the command and file name by manipulating the std::string library:
    std::size_t op_first_pos = cmd_text.find_first_of('>');
    try
    {
        left_cmd = SmallShell::getInstance().CreateCommand(cmd_text.substr(0, op_first_pos).c_str());
    }
    catch(const std::exception& e)
    {
        std::cerr << e.what() << std::endl;
    }
    filename = _trim(cmd_text.substr(op_first_pos + (type == CMD_Type::OutAppend) + 1));
}

RedirectionCommand::~RedirectionCommand()
{
    if(left_cmd)
    {
        delete left_cmd;
    }
}

void RedirectionCommand::prepare()
{
    if((stdout_backup = dup(STDOUT_FILENO)) == -1) // Backup the stdout channel
    {
        throw SyscallError("dup");
    }
    if(dup2(write_fd, STDOUT_FILENO) == -1) // Override the stdout channel with the file given by the user.
    {
        throw SyscallError("dup2");
    }
    if(close(write_fd) == -1) // Close the old fd of the file give by the user.
    {
        throw SyscallError("close");
    }
}

void RedirectionCommand::cleanup()
{
    if(dup2(stdout_backup, STDOUT_FILENO) == -1)
    {
        throw SyscallError("dup2");
    }
    if(close(stdout_backup) == -1)
    {
        throw SyscallError("close");
    }
}

void RedirectionCommand::execute()
{
    switch(type)
    {
        case CMD_Type::OutRed:
        {
            if((write_fd = open(filename.c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0666)) == -1)
            {
                throw SyscallError("open");
            }
            break;
        }
        case CMD_Type::OutAppend:
        {
            if((write_fd = open(filename.c_str(), O_CREAT | O_WRONLY | O_APPEND, 0666)) == -1)
            {
                throw SyscallError("open");
            }
            break;
        }
        default:
            return;
    }
    if(left_cmd)
    {
        prepare();

        left_cmd->execute();

        cleanup();
    }
    else
    {
        if(close(write_fd) == -1)
        {
            throw SyscallError("close");
        }
    }
}

PipeCommand::PipeCommand(const char *cmd_line, CMD_Type type) :
Command(cmd_line, false, 0, false), type(type), left_cmd(NULL), right_cmd(NULL) { }

PipeCommand::~PipeCommand()
{
    if(left_cmd)
    {
        delete left_cmd;
    }
    if(right_cmd)
    {
        delete right_cmd;
    }
}

void PipeCommand::execute()
{
    pid_t l_pid, r_pid;
    enum pipe_side { PIPE_R = 0, PIPE_W };
    std::size_t op_first_pos = cmd_text.find_first_of('|');
    int fd[2];
    
    if(pipe(fd) == -1)
    {
        throw SyscallError("pipe");
    }

    if((l_pid = fork()) == -1)
    {
        throw SyscallError("fork");
    }
    else if(l_pid == 0) // Child = Left command
    {
        setpgrp();
        try
        {
            if(dup2(fd[PIPE_W], type == CMD_Type::Pipe? STDOUT_FILENO : STDERR_FILENO) == -1)
            {
                throw SyscallError("dup2");
            }
            if(close(fd[PIPE_W]) == -1 || close(fd[PIPE_R]) == -1)
            {
                throw SyscallError("close");
            }
            left_cmd = SmallShell::getInstance().CreateCommand(cmd_text.substr(0, op_first_pos).c_str(), false);
            left_cmd->execute();
        }
        catch(const std::exception& e)
        {
            std::cerr << e.what() << std::endl;
            exit(1);
        }
        exit(0);
    }

    if((r_pid = fork()) == -1)
    {
        if(kill(l_pid, SIGKILL) == -1)
        {
            throw SyscallError("kill");
        }
        throw SyscallError("fork");
    }
    else if(r_pid == 0) // Child = Right command
    {
        setpgrp();
        try
        {
            if(dup2(fd[PIPE_R], STDIN_FILENO) == -1)
            {
                throw SyscallError("dup2");
            }
            if(close(fd[PIPE_W]) == -1 || close(fd[PIPE_R]) == -1)
            {
                throw SyscallError("close");
            }
            right_cmd = SmallShell::getInstance().CreateCommand(cmd_text.substr(op_first_pos + (type == CMD_Type::ErrPipe) + 1).c_str(), false);
            right_cmd->execute();
        }
        catch(const std::exception& e)
        {
            std::cerr << e.what() << std::endl;
            exit(1);
        }
        exit(0);
    }

    // Father = The main smash process
    if(close(fd[PIPE_W]) == -1 || close(fd[PIPE_R]) == -1)
    {
        throw SyscallError("close");
    }
    if(waitpid(l_pid, NULL, WUNTRACED) == -1)
    {
        throw SyscallError("waitpid");
    }
    if(waitpid(r_pid, NULL, WUNTRACED) == -1)
    {
        throw SyscallError("waitpid");
    }
}
//***************SMASH IMPLEMENTATION***************//
SmallShell::~SmallShell() { }

const std::string& SmallShell::getPrompt() const
{
    return prompt;
}

void SmallShell::setPrompt(const std::string& new_prompt)
{
    prompt = new_prompt;
}

pid_t SmallShell::getPid() const
{
    return main_pid;
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

std::shared_ptr<std::list<AlarmEntry>> SmallShell::getAlarmList()
{
    return alarm_list;
}

int SmallShell::getCurrentFgJobId() const
{
    return fg_job_id;
}

pid_t SmallShell::getCurrentFgPid() const
{
    return fg_pid;
}

void SmallShell::setCurrentFg(int job_id, pid_t j_pid)
{
    fg_job_id = job_id;
    fg_pid = j_pid;
}

//*************JOBSLIST IMPLEMENTATION*************//
std::shared_ptr<JobEntry> JobsList::addJob(Command *cmd, bool isStopped)
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
    std::shared_ptr<JobEntry> jcb_ptr = std::make_shared<JobEntry>(jcb);
    jobs[job_id] = jcb_ptr; // Add the new job to the jobs map. (AS A SHARED_PTR)
    if(!is_background && !isStopped)
    {
        SmallShell::getInstance().setCurrentFg(job_id, pid);
    }
    return jcb_ptr;
}

void JobsList::removeJob(int job_id)
{
    if(jobs.count(job_id))
    {
        jobs.erase(job_id);
    }
    if(job_id == SmallShell::getInstance().getCurrentFgJobId())
    {
        SmallShell::getInstance().setCurrentFg(0, 0);
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
            erase_list.push_front(jcb->job_id);
            if(jcb->job_id == smash.fg_job_id)
            {
                smash.setCurrentFg(0, 0);
            }
        }
    }

    for(auto& pair : jobs)
    {
        std::shared_ptr<JobEntry>& jcb = pair.second;
        status = 0;
        
        if(waitpid(jcb->pid, &status, WUNTRACED | WNOHANG) > 0) // If the job is stopped, update it.
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
        std::cout << "[" << jcb->job_id << "] " << jcb->command << " : " << jcb->pid << " " << difftime(now, jcb->start_time) << " secs" \
            << ((jcb->state == j_state::STOPPED)? " (stopped)" : "") << std::endl;
    }
}

void JobsList::killAllJobs(bool print)
{
    updateAllJobs();
    std::list<int> erase_list;
    if(print)
    {
        std::cout << "smash: sending SIGKILL signal to " << jobs.size() << " jobs:" << std::endl;
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

std::shared_ptr<JobEntry> JobsList::getJobByPid(pid_t j_pid)
{
    for(auto pair : jobs)
    {
        if(pair.second->pid == j_pid)
        {
            return pair.second;
        }
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
    while(last != jobs.rend() && last->second->state != j_state::STOPPED) last++;
    if(last == jobs.rend())
    {
        return nullptr;
    }
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