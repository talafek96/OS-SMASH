#include <iostream>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include "Commands.h"
#include "signals.h"


int main(int argc, char* argv[]) 
{
    struct sigaction sa_z, sa_c, sa_t;

    sa_z.sa_flags = SA_RESTART;
    sigemptyset(&sa_z.sa_mask);
    sa_z.sa_handler = ctrlZHandler;

    sa_c.sa_flags = SA_RESTART;
    sigemptyset(&sa_c.sa_mask);
    sa_c.sa_handler = ctrlCHandler;

    sa_t.sa_flags = SA_RESTART;
    sigemptyset(&sa_t.sa_mask);
    sa_t.sa_handler = alarmHandler;

    if(sigaction(SIGTSTP, &sa_z, NULL) == -1)
    {
        perror("smash error: failed to set ctrl-Z handler");
    }
    if(sigaction(SIGINT, &sa_c, NULL) == -1)
    {
        perror("smash error: failed to set ctrl-C handler");
    }
    if(sigaction(SIGALRM, &sa_t, NULL) == -1)
    {
        perror("smash error: failed to set timeout handler");
    }

    SmallShell& smash = SmallShell::getInstance();
    while(!smash.getQuitFlag()) 
    {
        std::cout << smash.getPrompt() << "> ";
        std::string cmd_line;
        std::getline(std::cin, cmd_line);
        try
        {
            smash.executeCommand(cmd_line.c_str());
        }
        catch(const std::exception& e)
        {
            std::cerr << e.what() << std::endl;
        }
    }
    return 0;
}