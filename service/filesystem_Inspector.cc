#include "filesystem_Inspector.h"

Inspector::Inspector(const char* fifo_pathname)
{
    fifo = RD_Fifo(fifo_pathname) ; 
    buffer  = new char[READ_BUFFER_SIZE*2] ; 
    buffer2 = new char[READ_BUFFER_SIZE*4] ;

} ; 

Inspector~Inspector()
{
    delete[] buffer ;
    return ;  
};

bool 
Inspector::if_dopen(std::string &dir_path) 
{   
    struct stat dir_info ; 
    if(stat(dir_path , &dir_info)!=0)
    {
        return false ; 
    }
    if(!dir_info.st_mode & (S_IFDIR | DIR_MODE)) //检测一个文件是否为目录
    {
        return false ; 
    }
    return true ; 
};


bool 
Inspector::inspect_dir()
{

};

int 
Inspector::readline(int fd ,  char* buffer ) 
{
    
}

void 
Inspector::run()
{
    int n  ; 
    int writefd ; 
    
    int read_offset = 0; 
    //套接字未设置非阻塞
    while((n= readline(fifo.readfd , read_buffer , 512))> 0 )
    {
        
    }
};