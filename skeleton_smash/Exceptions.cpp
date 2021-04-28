#include "Exceptions.h"

/*********************************/
/*           Exception           */
/*********************************/
Exception::Exception() :
intro("smash error: "), message("Unknown error.") { }
const char* Exception::what() const noexcept
{
    return message.c_str();
}

/*********************************/
/*          SyscallError         */
/*********************************/
SyscallError::SyscallError(const std::string& syscall_name) :
syscall_name(syscall_name) 
{
    int curr_err = errno;
    message = intro + syscall_name + " failed: " + strerror(curr_err);
    errno = curr_err;
}

/*********************************/
/*            CmdError           */
/*********************************/
CmdError::CmdError(const std::string& cmd_name) :
cmd_name(cmd_name) { }

/*********************************/
/*           InvalidArgs         */
/*********************************/
InvalidArgs::InvalidArgs(const std::string& cmd_name) : CmdError(cmd_name) 
{
    message = intro + cmd_name + ": invalid arguments";
}
InvalidArgs::~InvalidArgs() { }

/*********************************/
/*         JobDoesNotExist       */
/*********************************/
JobDoesNotExist::JobDoesNotExist(const std::string& cmd_name, int job_id) : CmdError(cmd_name), job_id(job_id)
{
    message = intro + cmd_name + static_cast<std::string>(": job-id ") + std::to_string(job_id) + " does not exist";
}
JobDoesNotExist::~JobDoesNotExist() { }

/*********************************/
/*          OldPwdNotSet         */
/*********************************/
OldPwdNotSet::OldPwdNotSet(const std::string& cmd_name) : CmdError(cmd_name)
{
    message = intro + cmd_name + ": OLDPWD not set";
}
OldPwdNotSet::~OldPwdNotSet() { }

/*********************************/
/*           TooManyArgs         */
/*********************************/
TooManyArgs::TooManyArgs(const std::string& cmd_name) : CmdError(cmd_name)
{
    message = intro + cmd_name + ": too many arguments";
}
TooManyArgs::~TooManyArgs() { }