/* 
 * File:   Log.h
 * Author: mike
 *
 * Created on 30 September 2014, 10:47
 */

#include <string.h>
#include <iostream>

#ifndef LOG_H
#define	LOG_H

class Log {
public:
    Log();
//    Log(const Log& orig);
    virtual ~Log();
    
    static void record(std::string message){
        std::cout<<message<<"\r\n";
    }
private:

};

#endif	/* LOG_H */

