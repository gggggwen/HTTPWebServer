#ifndef FILESYSTEM_INSPECTOR_H
#define FILESYSTEM_INSPECTOR_H

#include<iostream>
#include<fstream>
#include<string>
#include"../ipc/fifo.h"
#ifndef READ_BUFFER_SIZE 
#define READ_BUFFER_SIZE 4096
#endif

#define DIR_MODE (S_IROTH | S_IRUSR | S_IRGRP) 
#define MAX_LINE 512

class Inspector
{
public:
    Inspector(std::string  pathname)  ;
    ~Inspector() ;

    void run() ; 
private:
    bool if_dexist(std::string & dir_path ) ;
    bool if_dread (std::string & dir_path ) ;
    void inspect_dir()  ;  
private:
    char* buffer ; 
    char* buffer2 ;
    std::string shell ;
    RD_Fifo fifo ;  
} ; 


#endif