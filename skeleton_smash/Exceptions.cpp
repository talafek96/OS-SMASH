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
}

/*********************************/
/*            CmdError           */
/*********************************/
CmdError::CmdError(const std::string& cmd_name) :
cmd_name(cmd_name) { }

