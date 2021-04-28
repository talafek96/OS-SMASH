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
    virtual ~Exception() { }
    const char* what() const noexcept override;
};

class SyscallError : public Exception
{
protected:
    std::string syscall_name;
public:
    explicit SyscallError(const std::string& syscall_name);
    virtual ~SyscallError() { }
};

class CmdError : public Exception
{
protected:
    std::string cmd_name;
public:
    explicit CmdError(const std::string& cmd_name);
    virtual ~CmdError() { }
};

class InvalidArgs : public CmdError
{
public:
    explicit InvalidArgs(const std::string& cmd_name);
    virtual ~InvalidArgs();
};

class TooManyArgs : public CmdError
{
public:
    explicit TooManyArgs(const std::string& cmd_name);
    virtual ~TooManyArgs();
};

class JobDoesNotExist : public CmdError
{
    int job_id;
public:
    explicit JobDoesNotExist(const std::string& cmd_name, int job_id);
    virtual ~JobDoesNotExist();
};

class OldPwdNotSet : public CmdError
{
public:
    explicit OldPwdNotSet(const std::string& cmd_name);
    virtual ~OldPwdNotSet();
};

#endif