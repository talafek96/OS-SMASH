#ifndef SMASH_COMMAND_H_
#define SMASH_COMMAND_H_

#include <memory>
#include <queue>
#include <set>
#include <time.h>
#include <map>
#include <list>

#define COMMAND_ARGS_MAX_LENGTH (200)
#define COMMAND_MAX_ARGS (20)

void _removeBackgroundSign(char *cmd_line);
bool _isBackgroundCommand(const char *cmd_line);
void arrayFree(char **arr, int len);
bool isNumber(const std::string& str, bool is_unsigned = false);
bool extractIntFlag(const std::string& str, int* flag_num);

enum class CMD_Type
{
    Normal, Background, Pipe, ErrPipe,
    OutRed, OutAppend
};

class Command
{
protected:
    std::string cmd_text;
    pid_t pid = 0;
    bool is_background = false;
    bool valid_job = true;
    bool to_wait = true;

public:
    Command(const char* cmd_line, bool is_background, int pid = 0, bool valid_job = true, bool to_wait = true) : 
    cmd_text(cmd_line), pid(pid), is_background(is_background), valid_job(valid_job), to_wait(to_wait) { }
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

class ExternalCommand : public Command // DONE: external commands handler
{
protected:
    bool is_background;
    std::string stripped_cmd;

public:
    ExternalCommand(const char* cmd_line, bool is_background, bool valid_job, bool to_wait = true);
    virtual ~ExternalCommand() { }
    void execute() override;
};

class TimeoutCommand : public ExternalCommand // DONE: timeout command
{
    int duration;
    Command* command;

public:
    TimeoutCommand(const char* cmd_line, bool is_background, int duration, bool valid_job);
    virtual ~TimeoutCommand() = default;
    void execute() override;
};

class PipeCommand : public Command
{
    CMD_Type type;
    Command* left_cmd;
    Command* right_cmd;

public:
    PipeCommand(const char *cmd_line, CMD_Type type);
    virtual ~PipeCommand();
    void execute() override;
};

class RedirectionCommand : public Command
{
    CMD_Type type;
    Command* left_cmd;
    int stdout_backup;
    int write_fd;
    std::string filename;

    void prepare();
    void cleanup();

public:
    explicit RedirectionCommand(const char *cmd_line, CMD_Type type);
    virtual ~RedirectionCommand();
    void execute() override;
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
    std::shared_ptr<JobEntry> addJob(Command *cmd, bool isStopped = false);
    void removeJob(int job_id);
    void updateAllJobs();
    void printJobsList();
    void killAllJobs(bool print=true);
    std::shared_ptr<JobEntry> getJobById(int jobId);
    std::shared_ptr<JobEntry> getJobByPid(pid_t j_pid);
    bool killJobById(int jobId, bool to_update = true);
    std::shared_ptr<JobEntry> getLastJob(int *lastJobId);
    std::shared_ptr<JobEntry> getLastStoppedJob(int *jobId = NULL);
    bool isEmpty() const;
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
    // DONE: fg
    int job_id_to_fg=0;
public:
    ForegroundCommand(const char *cmd_line);
    virtual ~ForegroundCommand() {}
    void execute() override;
};

class BackgroundCommand : public BuiltInCommand
{
    // DONE: bg
    int job_id_to_bg=0;
public:
    BackgroundCommand(const char *cmd_line);
    virtual ~BackgroundCommand() {}
    void execute() override;
};

class CatCommand : public BuiltInCommand // DONE: cat
{
    std::queue<std::string> f_queue;
public:
    CatCommand(const char *cmd_line);
    virtual ~CatCommand() { }
    void execute() override;
};

//*****************ALARM CLASS*****************//

struct AlarmEntry
{
    time_t finish_time;
    pid_t pid;
    std::string cmd_text;

    bool operator<(const AlarmEntry& other) const
    {
        return (this->finish_time - other.finish_time) < 0;
    }

    bool operator==(const AlarmEntry& other) const
    {
        return (this->finish_time == other.finish_time);
    }

    bool operator<=(const AlarmEntry& other) const
    {
        return *this < other || *this == other;
    }

    bool operator>(const AlarmEntry& other) const
    {
        return !(*this <= other);
    }

    bool operator>=(const AlarmEntry& other) const
    {
        return *this > other || *this == other;
    }

    bool operator!=(const AlarmEntry& other) const
    {
        return !(*this == other);
    }
};

//*****************SMASH CLASS*****************//
class SmallShell
{
private:
    pid_t main_pid;
    std::string prompt;
    bool quit_flag = false;
    std::string last_pwd;
    std::shared_ptr<JobsList> jobs;
    std::shared_ptr<std::list<AlarmEntry>> alarm_list;
    int fg_job_id;
    pid_t fg_pid;

    const std::set<std::string> builtin_set;
    
    SmallShell() : main_pid(getpid()), prompt("smash"), quit_flag(false), last_pwd(""), jobs(std::make_shared<JobsList>(JobsList())),
    alarm_list(std::make_shared<std::list<AlarmEntry>>(std::list<AlarmEntry>())), fg_job_id(0),
    builtin_set({"chprompt", "showpid", "pwd", "cd", "jobs", "kill", "fg", "bg", "quit", "cat"}) {}
    
    void setLastPwd(const std::string& new_pwd);

    friend QuitCommand;
    friend ChangeDirCommand;
    friend JobsList;
    friend void ctrlZHandler();

public:
    Command *CreateCommand(const char *cmd_line, bool valid_job = true, bool to_wait = true);
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
    std::shared_ptr<std::list<AlarmEntry>> getAlarmList();
    const std::string& getPrompt() const; // get the prompt
    pid_t getPid() const; // get the main instance's pid
    void setPrompt(const std::string& new_prompt); // set the prompt to new_prompt
    bool isBuiltIn(const char* cmd_line) const;
    bool getQuitFlag() const;

    bool isPwdSet() const;
    const std::string& getLastPwd() const;
    int getCurrentFgJobId() const;
    pid_t getCurrentFgPid() const;
    void setCurrentFg(int job_id, pid_t j_pid);
};

#endif //SMASH_COMMAND_H_
