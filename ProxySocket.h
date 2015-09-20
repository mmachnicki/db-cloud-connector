/* 
 * File:   proxySocket.h
 * Author: mike
 *
 * Created on 30 September 2014, 10:07
 */

#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <unistd.h>
#include <strings.h>
#include <sstream>
#include <vector>
#include <algorithm>
#include <cstdio>
#include <bitset>
#include <math.h>

#include <errno.h>

#ifndef PROXYSOCKET_H
#define	PROXYSOCKET_H

#include "Log.h"
#include "Client.h"

class ProxySocket {
public:
    ProxySocket();
    virtual ~ProxySocket();

    bool initialise(int iPort);
    void run();
    bool stop();

protected:
    bool disconnect(int iSocket);
    bool closeSocket(int iSocket);
    Client *getClientBySocket(int iSocket);
    Client *getClientByID(char *id);
    bool sendData(int iSocket, char* pcMessageBuffer, int iMessageLength, char *requestID, char *responseID, char *metacommand);
    Client* connectClient(int socket);
    char* uniqueID(int iLength);
    int getMaxSocketFD(int iSockVectorSize);
    void getFDCopy(fd_set &fdSet, int iSockVectorSize);
    int receive(int sourcePipeDescriptor, Client *);
    bool processRequest(Client *client, char *requestID, char *metacommand);
    bool processResponse(Client *client, char *requestID, char *responseID, char *metacommand);
    

private:
    void fetchAllClientsSockets(std::vector<int> &vBuffer);
    int getPayloadSize(char* cSize);
    void processMetaCommand(Client *client, char *metacommand);
    bool compareCStr(char *, char *);
    

private:
    int iPort;
    struct sockaddr_in serverSocketDescriptor;
    struct sockaddr_un phpPipeDescriptor;
    int iMasterSocket, iPHPPipe;
    std::vector<int> vSockets;
    std::vector<Client *> vClients;    
};

#endif	/* PROXYSOCKET_H */