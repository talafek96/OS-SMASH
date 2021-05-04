#include <iostream>
#include <signal.h>
#include <unistd.h>
#include <sys/wait.h>
#include "signals.h"
#include "Commands.h"

using namespace std;

void ctrlZHandler(int sig_num) // Stop signal
{
    int backup_errno = errno;
    SmallShell& smash = SmallShell::getInstance();
    if(write(STDOUT_FILENO, "smash: got ctrl-Z\n", 18) == -1)
    {
        perror("smash error: write failed");
        errno = backup_errno;
        return;
    }
    smash.getJobsList()->updateAllJobs();
    int job_id = smash.getCurrentFgJobId();
    pid_t to_stop;
    if(job_id)
    {
        auto jcb = smash.getJobsList()->getJobById(job_id);
        to_stop = jcb->pid;
        if(kill(to_stop, SIGSTOP) != -1)
        {
            std::cout << "smash: process " << to_stop << " was stopped\n";
            jcb->start_time = time(NULL);
            jcb->state = j_state::STOPPED;
        }
        else
        {
            perror("smash error: kill failed");
        }
        smash.setCurrentFg(0, 0);
    }
    else if(to_stop = smash.getCurrentFgPid()) // The FG process is not a valid job, but does indeed run in the fg. Perhaps, timeout?
    {
        if(kill(to_stop, SIGSTOP) != -1)
        {
            std::cout << "smash: process " << to_stop << " was stopped\n";
        }
        else
        {
            perror("smash error: kill failed");
        }
        smash.setCurrentFg(0, 0);
    }
    errno = backup_errno;
}

void ctrlCHandler(int sig_num) // Kill signal
{
    int backup_errno = errno;
    SmallShell& smash = SmallShell::getInstance();
    if(write(STDOUT_FILENO, "smash: got ctrl-C\n", 18) == -1)
    {
        perror("smash error: write failed");
        errno = backup_errno;
        return;
    }
    smash.getJobsList()->updateAllJobs();
    pid_t to_kill;
    if(smash.getCurrentFgJobId())
    {
        to_kill = smash.getJobsList()->getJobById(smash.getCurrentFgJobId())->pid;
        if(smash.getJobsList()->killJobById(smash.getCurrentFgJobId(), false))
        {
            std::cout << "smash: process " << to_kill << " was killed\n";
        }
        smash.setCurrentFg(0, 0);
    }
    else if(to_kill = smash.getCurrentFgPid())
    {
        if(kill(to_kill, SIGKILL) != -1)
        {
            std::cout << "smash: process " << to_kill << " was killed\n";
        }
        else
        {
            perror("smash error: kill failed");
        }
        smash.setCurrentFg(0, 0);
    }
    errno = backup_errno;
}

void alarmHandler(int sig_num)
{
    int backup_errno = errno;
    auto alarm_list = SmallShell::getInstance().getAlarmList();
    time_t first_time = alarm_list->front().finish_time;
    time_t curr_time = time(NULL);
    SmallShell& smash = SmallShell::getInstance();
    int wait_ret = -2;
    pid_t to_alarm;
    std::shared_ptr<JobEntry> jcb = nullptr;

    std::cout << "smash: got an alarm\n";

    while((alarm_list->front().finish_time == first_time) || alarm_list->front().finish_time == curr_time)
    {
        to_alarm = alarm_list->front().pid;
        // = The process is still alive = not a zombie. (wait_ret == 0)
        // Or, the process is not alive. It either just died, and was not polled yet (wait_ret > 0)
        if((wait_ret = waitpid(to_alarm, NULL, WNOHANG)) >= 0) 
        {
            if(kill(to_alarm, SIGKILL) != -1)
            {
                std::cout << "smash: " << alarm_list->front().cmd_text << " timed out!\n";
                if(wait_ret > 0)
                {
                    jcb = smash.getJobsList()->getJobByPid(alarm_list->front().pid);
                    if(jcb)
                    {
                        smash.getJobsList()->removeJob(jcb->job_id);
                    }
                }
            }
            else
            {
                perror("smash error: kill failed");
            }
        }
        // else: The process was already reaped by another waitpid. Just print the alarm message for the pid and do not send sigkill.
        
        if(smash.getCurrentFgPid() == to_alarm)
        {
            smash.setCurrentFg(0, 0);
        }
        alarm_list->pop_front();
    }

    // Setup an alarm for the next process in the list.
    time_t next_time = alarm_list->front().finish_time;
    alarm(difftime(next_time, curr_time));

    errno = backup_errno;
}
