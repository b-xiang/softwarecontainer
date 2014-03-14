/*
 *   Copyright (C) 2014 Pelagicore AB
 *   All rights reserved.
 */
#ifndef FIFOIPC_H
#define FIFOIPC_H

#include <string>

#include "ipcmessage.h"

/*! FifoIPC is an implementation of an IPC based on a named pipe.
 *
 * The purpose of the IPC is to let Pelagicontain communicate with Controller.
 */
class FifoIPC
{
public:
    FifoIPC(IPCMessage &message);
    ~FifoIPC();

    /*! This call makes FifoIPC create a fifo and enter a loop to wait for
     *  input. Will return on error or when Controller has shut down the app.
     *
     * \param fifoPath A path to the fifo
     *
     * \return true if loop exited normally, false if there was any problem
     *          setting up or opening the fifo.
     */
    bool initialize(const std::string &fifoPath);

private:
    /* A call to this method will make us enter a loop waiting for input
     * and will only exit when there is an error or when the input
     * resulting in a call to shutdown has been received and the call to
     * shutdown has finished.
     */
    bool loop();

    bool createFifo();

    IPCMessage &m_message;
    std::string m_fifoPath;
    bool m_fifoCreated;
};

#endif //FIFOIPC_H
