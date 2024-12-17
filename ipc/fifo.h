#ifndef FIFO_H
#define FIFO_H

#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <stdio.h>

#define FILE_MODE (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH) 

class RD_Fifo
{
public:
    RD_Fifo(std::string  pathname)   
    {
        if(mkfifo(pathname.c_str() , FILE_MODE) < 0 && (errno != EEXIST))
        {
            std::cout << "can not create " << pathname <<std::endl;  
            exit(1) ; 
        }
        readfd = open(pathname.c_str() , O_RDONLY , 0) ; 
        name = pathname ; 
    } ;
    ~RD_Fifo()
    {
        close(readfd) ; 
    };
private:
    int   readfd   ; 
    std::string  name ; 
};

#endif