#pragma once

#include <cstring>
#include <memory>
#include <optional>
#include <string_view>
#include "io_context.hh"
#include "block_syscall.hh"
#include "socket_accept_operation.hh"
#include "socket_recv_operation.hh"
#include "socket_send_operation.hh"
#include "file_read_operation.hh"
#include <fcntl.h>
#include <iostream>
#include <unistd.h>

class AsyncFileDescriptor
{
public:
    /* Listen tcp non blocking socket */
    AsyncFileDescriptor(IOContext& io_context): io_context_{io_context} 
    {}
    AsyncFileDescriptor(const AsyncFileDescriptor&) = delete;
    AsyncFileDescriptor(AsyncFileDescriptor&& socket)
    : io_context_{socket.io_context_}
    , fd_{socket.fd_}
    , io_state_{socket.io_state_}
    , io_new_state_{socket.io_new_state_}
    {
        socket.fd_ = -1;
    }

    AsyncFileDescriptor(int fd, IOContext& io_context)
    : io_context_ {io_context}
    , fd_{fd}
    {
        std::cout << "AsyncFileDescriptor " << fd << "Created" << std::endl;
        fcntl(fd_, F_SETFL, O_NONBLOCK);
        //io_context_.attach(this);
    }

    ~AsyncFileDescriptor()
    {
        std::cout << "------delete the AsyncFileDescriptor(" << fd_ << ")\n";
        if (fd_ == -1)
            return;
        io_context_.detach(this);
        close(fd_); 
    }

    bool resumeRecv()
    {
        if (!coroRecv_)
        {
            std::cout<<" nada que resumir en receive " << std::endl;
            return false;
        }
        coroRecv_.resume();
        return true;
    }

    bool resumeSend()
    {
        if (!coroSend_)
        {
            std::cout<<" nada que resumir en el envio " << std::endl;
            return false;
        }
        coroSend_.resume();
        return true;
    }

//protected:
    friend SocketAcceptOperation;
    friend SocketRecvOperation;
    friend SocketSendOperation;
    friend FileReadOperation;
    friend IOContext;
    IOContext& io_context_;
    int fd_ = -1;
    uint32_t io_state_ = 0;
    uint32_t io_new_state_ = 0;
    

    std::coroutine_handle<> coroRecv_;
    std::coroutine_handle<> coroSend_;
};
