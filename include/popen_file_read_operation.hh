/*
 * Shared State
 *
 * Copyright (c) 2023  Javier Jorge <jjorge@inti.gob.ar>
 * Copyright (c) 2023  Instituto Nacional de Tecnología Industrial
 * Copyright (C) 2023  Asociación Civil Altermundi <info@altermundi.net>
 *
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU Affero General Public License as published by the
 * Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.
 * See the GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 */
#pragma once

#include <sys/socket.h>
#include <sys/types.h>
#include"popen_file_read_operation.hh"
#include "block_syscall.hh"

class PopenAsyncCommand;

/**
 * @brief Pipe Read Operation
 * 
 */
class PopenFileReadOperation : public BlockSyscall<PopenFileReadOperation, ssize_t>
{
public:
    PopenFileReadOperation(PopenAsyncCommand* socket, uint8_t* buffer, std::size_t len,std::shared_ptr<std::error_condition> ec=nullptr);
    ~PopenFileReadOperation();

    ssize_t syscall();
    void suspend();
private:
    PopenAsyncCommand* socket;
    uint8_t* mBuffer_;
    std::size_t len_;
};
