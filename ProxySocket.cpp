/* 
 * File:   DBSocket.cpp
 * Author: mike
 * 
 * Created on 30 September 2014, 10:07
 */


#include "ProxySocket.h"

#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#define SOCKET_SUCCESS true
#define SOCKET_FAILURE false

#define DB_PROVIDER_CLIENT_ID "DB_CLIENT" 
#define MESSAGE_MAX_SIZE 2000000

#define NO_DATA_TO_SEND "no_data"

/*
 * socket frame:
 *  ________________________________________________________________________________________________________________
 * |   [4 bytes]    |   [30 bytes]   |   [30 bytes]   |   [64 bytes]   |                                            |
 * | payload length |   request ID   |  response ID   |  meta command  |up to 4294967163 bytes for payload length   |
 * |________________|________________|________________|________________|____________________________________________|
 * 
 * Max frame length 2^32, effective payload size = 4294967163:
 * 2^32 - payload length - requestID - responseID - metacommand - null termination
 * 
 * requestID - id of a client making a request
 * responseID - id of a response, will match requestID, 
 * 
 * meta commands:
 * stop                 - stops the server
 * masterdb_off         - tells that the remote system is not available, requesting client is supposed to throw exception on its own system
 * 
 */
#define METACOMMAND_DISCONNECT "stop"
#define METACOMMAND_MASTERDB_OFF "masterdb_off"
#define METACOMMAND_I_AM_MASTERDB_PROVIDER "iammasterdb"
#define METACOMMAND_KEEPALIVE "keepalive"

#define CLIENT_HEADER_PAYLOAD_LENGTH 4          //4 bytes for size of the payload
#define CLIENT_HEADER_REQUESTID_LENGTH 30       //30 bytes for requestID
#define CLIENT_HEADER_RESPONSEID_LENGTH 30      //30 bytes for responseID
#define CLIENT_HEADER_METACOMMAND_LENGTH 64     //64 bytes for size of the meta command

using namespace std;

ProxySocket::ProxySocket() {
    this->iMasterSocket = INVALID_SOCKET;
    this->iPHPPipe = INVALID_SOCKET;
}

//DBSocket::DBSocket(const DBSocket& orig) {
//}

ProxySocket::~ProxySocket() {
    this->stop();
}

////////////////////////////////////////////////////////general operation of the socket server/////////////////////////
bool ProxySocket::initialise(int iPort) { 
    this->iPort = iPort;
    int iResult = 0;
    int socketHandle = INVALID_SOCKET;
    int iSetOpt = 1;

    socketHandle = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    setsockopt(socketHandle, SOL_SOCKET, SO_REUSEADDR, (char *) &iSetOpt, sizeof (iSetOpt));
    if (socketHandle == INVALID_SOCKET) {
        Log::record("Error creating socket.");

        return false;
    }
        
    this->serverSocketDescriptor.sin_family = AF_INET;
    this->serverSocketDescriptor.sin_port = htons(this->iPort);
    this->serverSocketDescriptor.sin_addr.s_addr = INADDR_ANY;
    bzero(&(this->serverSocketDescriptor.sin_zero), 8);//sizeof (this->sockDescriptor.sin_zero)
    
    iResult = bind(socketHandle, (struct sockaddr *) &this->serverSocketDescriptor, sizeof (struct sockaddr));
    if (iResult == SOCKET_ERROR) {
        Log::record("Error binding socket.");

        return false;
    }
    
    iResult = listen(socketHandle, 5);
    if (iResult == INVALID_SOCKET) {
        Log::record("Socket listen error.");

        return false;
    }
    this->iMasterSocket = socketHandle;
    this->vSockets.push_back(this->iMasterSocket);
        
    return true;
}

/*
 * Ensures new clients are connected and serves full duplex communication between multiple TCP concurrent
 * socket connections and named PHP pipe
 */
void ProxySocket::run(){
    int iSockCount = 0, iMaxFD = 0;
    fd_set socketReadSet;
    int iActiveSockCount = 0;    
    struct sockaddr_in cliAddr;    
    socklen_t cliLen = sizeof(cliAddr);
//    struct timeval tv;  //timeout for select
//    tv.tv_sec = 0;
//    tv.tv_usec = 5000000;    
            
    while(true){
        iSockCount = this->vSockets.size();
        iMaxFD = this->getMaxSocketFD(iSockCount);
        
        FD_ZERO(&socketReadSet);
        this->getFDCopy(socketReadSet, iSockCount);
        
        iActiveSockCount = select(iMaxFD+1, &socketReadSet, NULL, NULL, NULL);

        if (iActiveSockCount > 0) {
            int iActiveSocket, iSock;

            for (int i = 0; i < iSockCount; i++) {
                if (FD_ISSET(this->vSockets[i], &socketReadSet)) {
                    iActiveSocket = this->vSockets[i];

                    if (iActiveSocket == this->iMasterSocket) { //new connection is coming, register new client
                        iSock = accept(iActiveSocket, (struct sockaddr *) &cliAddr, &cliLen);
                        if (iSock == SOCKET_ERROR) {
                            Log::record("Create new socket failed with error:");
                            Log::record(strerror(errno));
                        } else this->connectClient(iSock);
                    } else {                        
                        if(this->receive(iActiveSocket, this->getClientBySocket(iActiveSocket)) == INVALID_SOCKET){
                            stringstream ssError;
                            ssError<<"Error reading from client, disconnecting client with associated socket "<<iActiveSocket;
                            Log::record(ssError.str());
                            
                            this->closeSocket(iActiveSocket);
                        }
                    }
                }
            }
        }
    }
}            

int ProxySocket::receive(int iReadSocket, Client *client) {
    if(client == NULL){
        return INVALID_SOCKET;
    }
    
    char cSize[CLIENT_HEADER_PAYLOAD_LENGTH];
    char cRequestID[CLIENT_HEADER_REQUESTID_LENGTH];
    char cResponseID[CLIENT_HEADER_RESPONSEID_LENGTH];
    char cMetacommand[CLIENT_HEADER_METACOMMAND_LENGTH];
    
    char *cpRequestID = &cRequestID[0];
    char *cpResponseID = &cRequestID[0];
    char *cpMetacommand = &cMetacommand[0];
    
    int iPayloadSize = 0;
    ssize_t iRecvBytes = 0;   

    int iResult; 
        
    //new transmission started, we need to read message header:
    //payload length (4 bytes), 
    //requestID (30 bytes),
    //responseID (30 bytes),
    //metaCommand (64 bytes)
    if(client->getCurrentBufferSize() == 0){        
        iResult = recv(iReadSocket, cSize, CLIENT_HEADER_PAYLOAD_LENGTH, MSG_WAITALL);
        iPayloadSize = this->getPayloadSize(cSize);
        if(iPayloadSize < 0){
            Log::record("Message length error. Message dumped.");
            
            return SOCKET_ERROR;
        }
        if(iPayloadSize > MESSAGE_MAX_SIZE){
            Log::record("Message too large. Message dumped.");
            
            return SOCKET_ERROR;
        }
                
        iResult = recv(iReadSocket, cRequestID, CLIENT_HEADER_REQUESTID_LENGTH, MSG_WAITALL);
        if(iResult == SOCKET_ERROR){
            Log::record("Error reading requestID from header.");
            
//            return SOCKET_ERROR;
        }
        
        iResult = recv(iReadSocket, cResponseID, CLIENT_HEADER_RESPONSEID_LENGTH, MSG_WAITALL);
        if(iResult == SOCKET_ERROR){
            Log::record("Error reading requestID from header.");
            
//            return SOCKET_ERROR;
        }
        
        iResult = recv(iReadSocket, cMetacommand, CLIENT_HEADER_METACOMMAND_LENGTH, MSG_WAITALL);
        if(iResult == SOCKET_ERROR){
            Log::record("Error reading metacommand from header.");
            
//            return SOCKET_ERROR;
        }
        
        this->processMetaCommand(client, cMetacommand);
        
        client->setBuffer(iPayloadSize);
    
    }else{//transmission continued from previous socket select route, message must have been fragmented
        iPayloadSize = client->getPendingBytesToRead();
    }    
    
    char cBuffer[iPayloadSize+1];
    if(iPayloadSize > 0) iRecvBytes = recv(iReadSocket, cBuffer, iPayloadSize, MSG_DONTWAIT);
        
    if (iRecvBytes > 0) {
        client->append(cBuffer, iRecvBytes);
//
//        shows what clients are talking about
//        stringstream ss;
//        ss<<client->getSocketHandle()<<"::"<<client->getBuffer();
//
//	Log::record(ss.str());
    }
    
    //client disconnected but didn's say a word. It still exists in the server's list with its file descriptor set to read ready
    if(iRecvBytes <= 0 && iPayloadSize > 0){
        
        return -1;
    }
    
    if(client->isReadyToBroadcast()){
        if(this->compareCStr(cRequestID, cResponseID)) this->processResponse(client, cpRequestID, cpResponseID, cMetacommand);
        else this->processRequest(client, (char *)client->getID(), cpMetacommand);
        client->resetBuffer();
    }
    if(iRecvBytes < 0){
        return iRecvBytes;
    }
    return iRecvBytes;
}

bool ProxySocket::processRequest(Client *client, char *requestID, char *cMetacommand){    
    Client *DBProvider = this->getClientByID((char *)DB_PROVIDER_CLIENT_ID);
    if(DBProvider == NULL){
        Log::record("Remote database system connection down, can not process requests."); 
        this->sendData(client->getSocketHandle(), NULL, 0, NULL, NULL, (char *)METACOMMAND_MASTERDB_OFF);
        
        return false;
    }
    if(this->compareCStr(requestID, (char *)DBProvider->getID())) return false;       //No loop back requests 
    int iResult;
    
    
    int iTalkToSocket = DBProvider->getSocketHandle();
    iResult = this->sendData(iTalkToSocket, client->getBuffer(), client->getCurrentBufferSize(), (char *)client->getID(), NULL, cMetacommand);
    
    if(iResult == SOCKET_FAILURE){
        Log::record("Problem with remote database system. Can't connect.");
        Log::record(strerror(errno));
        this->disconnect(iTalkToSocket);
        
        //let's tell the client the master db is no longer available        
        this->sendData(client->getSocketHandle(), NULL, 0, NULL, NULL, (char *)METACOMMAND_MASTERDB_OFF);
        
        return false;
    }
    return true;
}

bool ProxySocket::processResponse(Client* client, char* requestID, char *responseID, char *cMetacommand){        
    Client *receiver = this->getClientByID(requestID);    
    int iResult;
    stringstream ssError;
    
    if(receiver == NULL){
        ssError << "Client sending request "<<requestID<<" is no longer available.";
        this->disconnect(client->getSocketHandle());        
        
        return false;
    }
    
    int iTalkToSocket = receiver->getSocketHandle();
    if(client->getCurrentBufferSize() == 0){
        char *cBNoData = NO_DATA_TO_SEND;
        iResult = this->sendData(iTalkToSocket, cBNoData, 7, requestID, responseID, cMetacommand);
    }else if(client->getCurrentBufferSize() < 0){
        this->closeSocket(iTalkToSocket);
        
        return false;
    }else{
        iResult = this->sendData(iTalkToSocket, client->getBuffer(), client->getCurrentBufferSize(), requestID, responseID, cMetacommand);
    }
    if(iResult == SOCKET_FAILURE){
        Log::record("Problem with remote client. Can't connect.");
        Log::record(strerror(errno));
        
        //remove this line when server gets too busy, replace with queueing mechanism
        //Now it says: disconnect the db socket and let's just wait for the db system to connect again
//        this->disconnect(iTalkToSocket);
        this->closeSocket(iTalkToSocket);
        
        return false;
    }
    return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////



////////////////////////////////////////////////////////closing sockets//////////////////////////////////

/*
 * Closes all sockets, master socket including
 */
bool ProxySocket::stop(){ 
    bool bError;
    vector<int>::iterator viSocket;  
    
    Log::record("Closing all connections.");
    for(viSocket = this->vSockets.begin(); viSocket != this->vSockets.end(); viSocket++){          
        if(*viSocket != this->iMasterSocket){            
            if(this->disconnect(*viSocket) == SOCKET_FAILURE) bError = true;
        }        
    }    
    close(this->iMasterSocket);
    if(bError){
        Log::record("Closing with errors, some socket may be still connected.");
        
        return false;
    }
    return true;
}

bool ProxySocket::disconnect(int iSocket){    
    if(iSocket != this->iMasterSocket) return this->sendData(iSocket, NULL, 0, NULL, NULL, (char*)METACOMMAND_DISCONNECT);
    
    return this->closeSocket(iSocket);
}

bool ProxySocket::closeSocket(int iSocket){
    int n;
    if(iSocket == this->iMasterSocket) return false;
    Client *cl = this->getClientBySocket(iSocket);
    
    if(cl == NULL) return false;
    
    vector<Client *>::iterator newEndCl = remove(this->vClients.begin(), this->vClients.end(), cl);
    this->vClients.erase(newEndCl, this->vClients.end());
        
    vector<int>::iterator newEndS = remove(this->vSockets.begin(), this->vSockets.end(),iSocket);
    this->vSockets.erase(newEndS, this->vSockets.end());
    n = close(iSocket);
//    delete[] cl->getID();
    delete cl;
        
    if(n == 0){
        return true;
    }
    return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////sending data over the sockets/////////////////////////////
/*
 * Composes a frame and sends it
 */
bool ProxySocket::sendData(int iSocket, char* pcMessageBuffer, int iMessageLength, char *cRequestID, char *cResponseID, char *cMetacommand){    
    int iEnd = 4;
    int iResult;
    char cFrame[iMessageLength + CLIENT_HEADER_REQUESTID_LENGTH + CLIENT_HEADER_RESPONSEID_LENGTH + CLIENT_HEADER_METACOMMAND_LENGTH];
    
    cFrame[0] = (iMessageLength >> 24) & 0xFF;
    cFrame[1] = (iMessageLength >> 16) & 0xFF;
    cFrame[2] = (iMessageLength >> 8) & 0xFF;
    cFrame[3] = (iMessageLength) & 0xFF;
    
    for(int i=0; i<CLIENT_HEADER_REQUESTID_LENGTH; i++){
        if(cRequestID != NULL && cRequestID[i] != '\0'){
            cFrame[iEnd] = cRequestID[i];            
        }else{
            cFrame[iEnd] = 0x0;            
        }     
        iEnd ++;
    }
    
    for(int i=0; i<CLIENT_HEADER_RESPONSEID_LENGTH; i++){
        if(cResponseID != NULL && cResponseID[i] != '\0'){
            cFrame[iEnd] = cResponseID[i];
        }else{
            cFrame[iEnd] = 0x0;
        }
        iEnd++;
    }
     
    bool nMore = false;
    for(int i=0; i<CLIENT_HEADER_METACOMMAND_LENGTH; i++){
        if(cMetacommand != NULL && cMetacommand[i] != '\0' && !nMore) cFrame[iEnd] = cMetacommand[i];
        else{
            cFrame[iEnd] = 0x0;
            nMore = true;
        }
        iEnd++;        
    }
    
    for(int i=0; i<iMessageLength; i++){
        cFrame[iEnd] = pcMessageBuffer[i];
        iEnd++;
    }
    cFrame[iEnd] = 0x0;
    
    iResult = send(iSocket, cFrame, iEnd, MSG_NOSIGNAL);
    recv(iSocket, NULL, iEnd, MSG_DONTWAIT);

    if (iResult < 0){
        stringstream ss;
        ss<<"Socket "<<iSocket<<" write error";
        Log::record(ss.str());        
        this->closeSocket(iSocket);
        
        return false;
    }
    if (iResult == 0){
        stringstream ss;
        ss<<"Socket "<<iSocket<<" disconnected on client's side, closing socket.";
        Log::record(ss.str());            
        this->closeSocket(iSocket);
        
        return false;
    }
    return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////support elements///////////////////////////////////
Client* ProxySocket::connectClient(int iSocket) {
    if(iSocket <= 0) return NULL;
    int iClientIDLength = 30;    
    Client *client = new Client(this->uniqueID(iClientIDLength), iSocket);    
    
    this->vClients.push_back(client);
    this->vSockets.push_back(iSocket);  
    
//    shows id's of the clients when client connects
//    stringstream ss;
//    
//    ss<<"Client connected with ID="<<client->getID()<<";";
//    
//    Log::record(ss.str());
    
    return client;
}

Client* ProxySocket::getClientBySocket(int iSocket){    
    vector<Client*>::iterator cl;
    
    for(cl = this->vClients.begin(); cl<this->vClients.end(); ++cl){
        if((*cl)->getSocketHandle() == iSocket){
            return *cl;
        }
    }
    return NULL;
}

Client* ProxySocket::getClientByID(char *cID){    
    vector<Client*>::iterator cl;
    
    for(cl = this->vClients.begin(); cl<this->vClients.end(); ++cl){
        if(this->compareCStr((char *)(*cl)->getID(), cID)){
            return *cl;
        }
    }
    return NULL;
}

char* ProxySocket::uniqueID(int iLength){    
    char pool[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    int iMax = sizeof(pool);
    string sUniqueID;
    
    for(int i=0; i<iLength; i++){        
        sUniqueID += (pool[rand() % iMax]);
    }    
    char* cUniqueID = new char[iLength+1];
    
    return strcpy(cUniqueID, sUniqueID.c_str());
}

int ProxySocket::getMaxSocketFD(int iSockVectorSize){    
    int ifd = 0;
    
    for(int i=0; i<iSockVectorSize; i++){
        if(ifd < this->vSockets[i]) ifd = this->vSockets[i];
    }
    return ifd;
}

void ProxySocket::getFDCopy(fd_set &fdSet, int iSockVectorSize) {    
    for (int i = 0; i < iSockVectorSize; i++) {
        FD_SET(this->vSockets[i], &fdSet);
    }
}

int ProxySocket::getPayloadSize(char* cSize){     
    unsigned char c;
    string sres="";
    
    for(int i=0; i<CLIENT_HEADER_PAYLOAD_LENGTH; i++){
        c = cSize[i];
        bitset<8> b((int)c);
        string sb = b.to_string<char,char_traits<char>,allocator<char> >();
        sres += sb;        
    }    
    int power = 0;
    int iSize = 0;
    for(int i=CLIENT_HEADER_PAYLOAD_LENGTH*8-1; i>0; i--){
        stringstream cc;        
        cc<<sres[i];
        int ik;
        cc>>ik;
        iSize += pow(2, power)*ik;
        power += 1;
    }    
    return iSize;
}

void ProxySocket::processMetaCommand(Client *messenger, char* metacommand){
    if(this->compareCStr(metacommand, (char *)METACOMMAND_I_AM_MASTERDB_PROVIDER)){
        messenger->setID(DB_PROVIDER_CLIENT_ID);
    }
    if(this->compareCStr(metacommand, (char *)METACOMMAND_DISCONNECT)){
        this->stop();
        exit(0);
    }
    if(this->compareCStr(metacommand, (char *) METACOMMAND_KEEPALIVE)){
        char *cM = "{\"command\":\"keepalive\",\"parameters\":\"ok\"}";
        this->sendData(messenger->getSocketHandle(), cM, 41, NULL, NULL, NULL);
    }
}

bool ProxySocket::compareCStr(char *cSource, char *cCompareTo){
    if(cSource == NULL) return false;
    if(cSource[0] == '\0') return false;
    return strcmp(cSource, cCompareTo) == 0;
}

//////////////////////////////////////////////////////////////////////////////////////////////
