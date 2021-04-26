#include <unistd.h>
#include <string.h>
#include <iostream>
#include <vector>
#include <sstream>
#include <sys/wait.h>
#include <iomanip>
#include "Commands.h"

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
    OutRed, OutAppend, InRed,
    InAppend
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
    std::size_t pos = cmd.find_first_of("><|"); 
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
            case '<':
            {
                oper[0] = '<';
                if(cmd[pos+1] == '<')
                {
                    oper[1] = '<';
                    res = CMD_Type::InAppend;
                    goto DoubleOp;
                }
                else
                {
                    res = CMD_Type::InRed;
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
Command *SmallShell::CreateCommand(const char *cmd_line)
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
    // string cmd_s = _trim(string(cmd_line));
    // string firstWord = cmd_s.substr(0, cmd_s.find_first_of(" \n"));
    // if(firstWord.compare("chprompt") == 0)
    // {
    //     char *args[COMMAND_MAX_ARGS];
    //     int n = _parseCommandLine(cmd_line, args);

    //     arrayFree(args, n); // TODO: make an array delete function
    // }
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
                return new ChpromptCommand(cmd_line);
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
ChpromptCommand::ChpromptCommand(const char *cmd_line)
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