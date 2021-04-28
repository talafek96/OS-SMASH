#include <unistd.h>
#include <string.h>
#include <iostream>
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

SmallShell::~SmallShell()
{
    // TODO: add your implementation
}

/**
* Creates and returns a pointer to Command class which matches the given command line (cmd_line)
*/
Command* SmallShell::CreateCommand(const char *cmd_line)
{
    // For example:
    /*
  string cmd_s = _trim(string(cmd_line));
  string firstWord = cmd_s.substr(0, cmd_s.find_first_of(" \n"));

  if (firstWord.compare("pwd") == 0) {
    return new GetCurrDirCommand(cmd_line);
  }
  else if (firstWord.compare("showpid") == 0) {
    return new ShowPidCommand(cmd_line);
  }
  else if ...
  .....
  else {
    return new ExternalCommand(cmd_line);
  }
  */
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
    // TODO: Add your implementation here
    // for example:
    // Command* cmd = CreateCommand(cmd_line);
    // cmd->execute();
    // Please note that you must fork smash process for some commands (e.g., external commands....)
    CMD_Type type = _processCommandLine(cmd_line);
    switch(type)
    {
        case CMD_Type::Normal:
        {
            if(isBuiltIn(cmd_line))
            {
                Command* cmd = CreateCommand(cmd_line);
                cmd->execute();
                delete cmd;
            }
            else
            {
                // TODO: External commands excecuter.
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

//***************SMASH IMPLEMENTATION***************//
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
}

void JobsList::updateAllJobs()
{
    int status;
    for(auto& pair : jobs)
    {
        std::shared_ptr<JobEntry>& jcb = pair.second;
        status = 0;
        if(waitpid(jcb->pid, &status, WUNTRACED | WNOHANG) > 0) // If the job is dead, remove it.
        {
            if(WIFSTOPPED(status)) // If job is just stopped, update it's status.
            {
                jcb->state = j_state::STOPPED;
            }
            else // The job is dead, so remove it from the job container.
            {
                jobs.erase(jcb->job_id);
            }
        }
    }
}

void JobsList::printJobsList()
{
    updateAllJobs();
    for(auto& pair : jobs)
    {
        std::shared_ptr<JobEntry>& jcb = pair.second;
        std::cout << "[" << jcb->job_id << "]" << jcb->command << " : " << jcb->pid << " " << difftime(time(NULL), jcb->start_time) << "secs" \
            << (jcb->state == j_state::STOPPED)? " (stopped)\n" : "\n";
    }
}

void JobsList::killAllJobs(bool print)
{
    updateAllJobs();
    if(print)
    {
        std::cout << "smash: sending SIGKILL signal to " << jobs.size() << " jobs:\n";
    }
    for(auto& pair : jobs)
    {
        if(kill(pair.second->pid, SIGKILL) == -1)
        {
            perror("smash error: kill failed");
        }
        else
        {
            jobs.erase(pair.first); // Erase the job from the jobs map on success
        }
    }
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

void JobsList::killJobById(int jobId, bool to_update)
{
    if(to_update) updateAllJobs();

    if(jobs.count(jobId))
    {
        std::shared_ptr<JobEntry> jcb = jobs[jobId];
        if(kill(jcb->pid, SIGKILL) == -1)
        {
            perror("smash error: kill failed");
        }
        else
        {
            jobs.erase(jcb->job_id); // Erase the job from the jobs map on success
        }
        
    }
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

std::shared_ptr<JobEntry> JobsList::getLastStoppedJob(int *jobId = NULL)
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