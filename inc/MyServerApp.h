/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   MyServerApp.h
 * Author: LongLNT
 *
 * Created on August 14, 2018, 10:58 AM
 */
#ifndef MYSERVERAPP_H
#define MYSERVERAPP_H
#include <Poco/Util/ServerApplication.h>
#include <Poco/Util/OptionSet.h>
#include <vector>
#include <string>

//#include "dispatcher.h"

using namespace Poco::Util;

class MyServerApp : public ServerApplication {
public:
    MyServerApp();
    
    MyServerApp(const MyServerApp& orig);
    
    virtual ~MyServerApp();
    
    void defineOptions(OptionSet& options) {}
    
    int main(const std::vector<std::string> &) {}
    
private:
    //dispatcher _dispatcher;
};

#endif /* MYSERVERAPP_H */

