/* 
 * File:   main.cpp
 * Author: mike
 *
 * Created on 30 September 2014, 11:33
 */

#include <cstdlib>

#include "ProxySocket.h"

/*
 * 
 */
int main(int argc, char** argv) {
    ProxySocket *proxySocket = new ProxySocket();
    
    if(proxySocket->initialise(8080)) proxySocket->run();    

    return 0;
}

