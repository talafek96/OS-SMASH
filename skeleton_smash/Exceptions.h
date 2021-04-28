#ifndef _SMASH_EXCEPTIONS
#define _SMASH_EXCEPTIONS

#include <iostream>
#include <string.h>
#include <errno.h>

class Exception : public std::exception 
{ 
protected:
    std::string intro;
    std::string message;
public:
    Exception();
    virtual ~Exception() = default;
    const char* what() const noexcept override;
};

class SyscallError : public Exception
{
protected:
    std::string syscall_name;
public:
    explicit SyscallError(const std::string& syscall_name);
    virtual ~SyscallError() = default;
};

class CmdError : public Exception
{
protected:
    std::string cmd_name;
public:
    explicit CmdError(const std::string& cmd_name);
    virtual ~CmdError() = default;
};


#endif