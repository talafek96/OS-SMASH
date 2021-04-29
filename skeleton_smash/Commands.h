#ifndef SMASH_COMMAND_H_
#define SMASH_COMMAND_H_

#include <memory>
#include <vector>
#include <set>
#include <time.h>
#include <map>
#include <list>

#define COMMAND_ARGS_MAX_LENGTH (200)
#define COMMAND_MAX_ARGS (20)

void _removeBackgroundSign(char *cmd_line);
bool _isBackgroundCommand(const char *cmd_line);
void arrayFree(char **arr, int len);
bool isNumber(const std::string& str);
bool extractIntFlag(const std::string& str, int* flag_num);

class Command
{
protected:
    std::string cmd_text;
    pid_t pid = 0;
    bool is_background = false;

public:
    Command(const char* cmd_line, bool is_background, int pid = 0) : 
    cmd_text(cmd_line), pid(pid), is_background(is_background) { }
    virtual ~Command() = default;
    virtual void execute() = 0;
    virtual const std::string& getCmdLine() const;
    virtual int getPid() const;
    virtual bool isBackground() const;
    //virtual void prepare();
    //virtual void cleanup();
};

class BuiltInCommand : public Command
{
public:
    BuiltInCommand(const char* cmd_line) : Command(cmd_line, false) { }
    virtual ~BuiltInCommand() = default;
};

class ExternalCommand : public Command // TODO: external commands handler
{
    bool is_background;
    std::string stripped_cmd;

public:
    ExternalCommand(const char* cmd_line, bool is_background);
    virtual ~ExternalCommand() = default;
    void execute() override;
};

class PipeCommand : public Command
{
    // TODO: pipe
public:
    PipeCommand(const char *cmd_line);
    virtual ~PipeCommand() {}
    void execute() override;
};

class RedirectionCommand : public Command
{
    // TODO: Redirection - Output redirection
public:
    explicit RedirectionCommand(const char *cmd_line);
    virtual ~RedirectionCommand() {}
    void execute() override;
    //void prepare() override;
    //void cleanup() override;
};

class ChangeDirCommand : public BuiltInCommand // DONE: cd
{
    bool minus_arg = false;
    std::string pathname;
    
public:
    ChangeDirCommand(const char *cmd_line);
    virtual ~ChangeDirCommand() {}
    void execute() override;
};

class GetCurrDirCommand : public BuiltInCommand // DONE: pwd
{
public:
    GetCurrDirCommand(const char *cmd_line);
    virtual ~GetCurrDirCommand() {}
    void execute() override;
}; 

class ShowPidCommand : public BuiltInCommand // DONE: showpid
{
    pid_t pid;
public:
    ShowPidCommand(const char *cmd_line);
    virtual ~ShowPidCommand() {}
    void execute() override;
};

class JobsList;

class QuitCommand : public BuiltInCommand // DONE: quit
{
    bool is_kill=false;
    std::shared_ptr<JobsList> jobs;
public:
    QuitCommand(const char *cmd_line, std::shared_ptr<JobsList> jobs);
    virtual ~QuitCommand() {}
    void execute() override;
};

//******************JOBSLIST DATA STRUCTURE******************//

enum j_state
{
	RUNNING, STOPPED, ZOMBIE
};

struct JobEntry
{
    int job_id;
    int pid;
    std::string command;
    time_t start_time;
    j_state state;
    bool is_background;
};

class JobsList
{
    std::map<int, std::shared_ptr<JobEntry>> jobs;

public:
    JobsList() = default;
    ~JobsList() = default;
    void addJob(Command *cmd, bool isStopped = false);
    void updateAllJobs();
    void printJobsList();
    void killAllJobs(bool print=true);
    std::shared_ptr<JobEntry> getJobById(int jobId);
    void killJobById(int jobId, bool to_update = true);
    std::shared_ptr<JobEntry> getLastJob(int *lastJobId);
    std::shared_ptr<JobEntry> getLastStoppedJob(int *jobId);
};
//***********************************************************//

class ChpromptCommand : public BuiltInCommand // DONE: chprompt
{
    std::string new_prompt;
public:
    ChpromptCommand(const char *cmd_line);
    virtual ~ChpromptCommand() {}
    void execute() override;
};

class JobsCommand : public BuiltInCommand // DONE: jobs
{
    std::shared_ptr<JobsList> jobs;

public:
    JobsCommand(const char *cmd_line, std::shared_ptr<JobsList> jobs);
    virtual ~JobsCommand() {}
    void execute() override;
};

class KillCommand : public BuiltInCommand // DONE: kill
{
    int signum;
    int job_id;
    std::shared_ptr<JobsList> jobs;

public:
    KillCommand(const char *cmd_line, int signum, int job_id, std::shared_ptr<JobsList> jobs);
    virtual ~KillCommand() { }
    void execute() override;
};

class ForegroundCommand : public BuiltInCommand
{
    // TODO: fg
public:
    ForegroundCommand(const char *cmd_line, JobsList *jobs);
    virtual ~ForegroundCommand() {}
    void execute() override;
};

class BackgroundCommand : public BuiltInCommand
{
    // TODO: bg
public:
    BackgroundCommand(const char *cmd_line, JobsList *jobs);
    virtual ~BackgroundCommand() {}
    void execute() override;
};

class CatCommand : public BuiltInCommand // TODO: cat
{
public:
    CatCommand(const char *cmd_line);
    virtual ~CatCommand() {}
    void execute() override;
};

//*****************SMASH CLASS*****************//
class SmallShell
{
private:
    std::string prompt;
    bool quit_flag = false;
    std::string last_pwd;
    std::shared_ptr<JobsList> jobs;
    const std::set<std::string> builtin_set;

    SmallShell() : prompt("smash"), quit_flag(false), last_pwd(last_pwd), jobs(std::make_shared<JobsList>(JobsList())),
    builtin_set({"chprompt", "showpid", "pwd", "cd", "jobs", "kill", "fg", "bg", "quit", "cat"}) {} //TODO: SmallShell: Consider adding timeout
    
    void setLastPwd(const std::string& new_pwd);

    friend QuitCommand;
    friend ChangeDirCommand;

public:
    Command *CreateCommand(const char *cmd_line);
    SmallShell(SmallShell const &) = delete;     // disable copy ctor
    void operator=(SmallShell const &) = delete; // disable = operator
    static SmallShell &getInstance()             // make SmallShell singleton
    {
        static SmallShell instance; // Guaranteed to be destroyed.
        // Instantiated on first use.
        return instance;
    }
    ~SmallShell();
    void executeCommand(const char *cmd_line);

    std::shared_ptr<JobsList> getJobsList();
    const std::string& getPrompt() const; // get the prompt
    void setPrompt(const std::string& new_prompt); // set the prompt to new_prompt
    bool isBuiltIn(const char* cmd_line) const;
    bool getQuitFlag() const;

    bool isPwdSet() const;
    const std::string& getLastPwd() const;
};

#endif //SMASH_COMMAND_H_
