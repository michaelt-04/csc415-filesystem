/**************************************************************
* Class::  CSC-415-02 Spring 2025
* Name:: Randy Chen, Michael Thompson, Eric Ahsue, Utku Tarhan
* Student IDs:: 922525848, 922707016, 922711514, 918371654
* GitHub-Name:: Jasuv
* Group-Name:: Debug Thugs
* Project:: Basic File System
*
* File:: b_io.h
*
* Description:: Interface of basic I/O Operations
*
**************************************************************/

#ifndef _B_IO_H
#define _B_IO_H
#include <fcntl.h>

typedef int b_io_fd;

b_io_fd b_open (char * filename, int flags);
int b_read (b_io_fd fd, char * buffer, int count);
int b_write (b_io_fd fd, char * buffer, int count);
int b_seek (b_io_fd fd, off_t offset, int whence);
int b_close (b_io_fd fd);

#endif

