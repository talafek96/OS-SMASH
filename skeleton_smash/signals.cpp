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
    int job_id = smash.getCurrentFg();
    if(job_id)
    {
        auto jcb = smash.getJobsList()->getJobById(job_id);
        pid_t to_stop = jcb->pid;
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
        smash.setCurrentFg(0);
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
    if(smash.getCurrentFg())
    {
        pid_t to_kill = smash.getJobsList()->getJobById(smash.getCurrentFg())->pid;
        if(smash.getJobsList()->killJobById(smash.getCurrentFg(), false))
        {
            std::cout << "smash: process " << to_kill << " was killed\n";
        }
        smash.setCurrentFg(0);
    }
    errno = backup_errno;
}

void alarmHandler(int sig_num)
{
    // TODO: Add your implementation
}
