/* 
 * File:   Client.h
 * Author: mmachnicki
 *
 * Created on 17 June 2014, 14:45
 */

#include <stdlib.h>
#include <sstream>

class Client {
public:
    Client(const char* id, int socket);
    virtual ~Client();
    
public:
    const char* getID();
    int getSocketHandle();
    bool getHandshake();
    int getPid();
    void setID(const char* id);
    void setSocket(int socketHandle);
    void setHandshake(bool handshake);
    void setPid(int pid);
    /*
     * If client's buffer full, the clients sets itself into readyToBroadcast mode
     */
    bool isReadyToBroadcast();
    bool setBuffer(unsigned int size);
    bool resetBuffer();
    int append(char * message, int length);
    int getCurrentBufferSize();
    int getTotalPayloadSize();
    int getPendingBytesToRead();
    char *getBuffer();
    
private:
    const char* id;
    int socketHandle;
    bool handshake;
    int pid; 
    bool exists;
    std::string sBuffer;
    int iBufferSize;
    int iCurrentBufferBytes;    
};

