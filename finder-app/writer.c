/*
	Assignment 2 - Writer.c
	Allen Gonzalez
	CU Boulder ECEA 5305 
	
	C version of writer.sh.
	takes 2 arguments, file and string and writes to that file.
	
*/

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <syslog.h>


int main(int argc, char **argv)
{
    openlog("writer.c", 0, LOG_USER);

    /* First parameter (#0) is filename */
    if (argc != 3)
    {
        //Error if number of arguments is incorrect.
        syslog(LOG_ERR, "Need 2 arguments: writefile and writestr.");
        
        return 1;
    }
    
    const char* writefile = argv[1];
    const char* writestr = argv[2];

    
    FILE *file = fopen(writefile, "w");

    if ((file == NULL) || errno != 0)
    {
    	//Error if file cannot be opened
        syslog(LOG_ERR, "fopen failed.");
        return errno;
    }

    syslog(LOG_DEBUG, "Writing %s to %s\n", writestr, writefile);
    //Writes the string from argument 2
    fprintf(file, "%s", writestr);
    
    //Error if file cannot be modified
    if (errno != 0)
    {
        syslog(LOG_ERR, "fprintf failed");
        return errno;
    }

    return 0;
}
