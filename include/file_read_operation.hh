#pragma once

#include <sys/socket.h>
#include <sys/types.h>

#include "block_syscall.hh"

class AsyncFileDescriptor;

class FileReadOperation : public BlockSyscall<FileReadOperation, ssize_t>
{
public:
    FileReadOperation(AsyncFileDescriptor* socket, void* buffer, std::size_t len);
    ~FileReadOperation();

    ssize_t syscall();
    void suspend();
private:
    AsyncFileDescriptor* socket;
    void* buffer_;
    std::size_t len_;
};
