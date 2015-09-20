/* 
 * File:   Client.cpp
 * Author: mmachnicki
 * 
 * Created on 17 June 2014, 14:45
 */

#include "Client.h"
#include "Log.h"

#define BUFFER_OVERFILL -1
#define MAX_BUFFER_SIZE 4294967227 //4294967296 - 4 - 64 - 1 (2^32 - payload length - metacommand - null termination) 

Client::Client(const char* id, int socketHandle) {
    this->id = id;
    this->socketHandle = socketHandle;
    this->handshake = false;
    this->pid = -1;    
    this->exists = true;    
    this->iCurrentBufferBytes = 0;
    this->iBufferSize = 0;
    this->sBuffer = "";
}

Client::~Client() {
    this->resetBuffer(); 
}

bool Client::isReadyToBroadcast(){
    return this->iBufferSize == this->iCurrentBufferBytes;
}

bool Client::resetBuffer(){
    this->sBuffer = "";
    this->iBufferSize = 0;
    this->iCurrentBufferBytes = 0;
    
    return true;
}

bool Client::setBuffer(unsigned int iSize){    
    if(iSize >= MAX_BUFFER_SIZE || iSize < 0) return false;
    this->sBuffer = "";
    this->iBufferSize = iSize;
    
    return true;
}

int Client::append(char* cMessage, int iLength){
    if(this->iCurrentBufferBytes+iLength > this->iBufferSize) return BUFFER_OVERFILL;
    this->sBuffer.append(cMessage);
    this->iCurrentBufferBytes += iLength;
    
    return iLength;
}

int Client::getCurrentBufferSize(){
    return this->iCurrentBufferBytes;
}

int Client::getTotalPayloadSize(){
    return this->iBufferSize;
}

int Client::getPendingBytesToRead(){
    return this->iBufferSize - this->iCurrentBufferBytes;
}

char *Client::getBuffer(){
    return (char *)this->sBuffer.c_str();
}

const char* Client::getID(){
    return this->id;
}

int Client::getSocketHandle(){
    return this->socketHandle;
}

bool Client::getHandshake(){
    return this->handshake;
}

int Client::getPid(){
    return this->pid;
}

void Client::setID(const char* id){
    this->id = id;
}

void Client::setSocket(int socketHandle){
    this->socketHandle = socketHandle;
}

void Client::setHandshake(bool handshake){
    this->handshake = handshake;
}

void Client::setPid(int pid){
    this->pid = pid;
}
