#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "Typedefs.h"

FILE * getFileHandle(const char * name, const char * mode, bool mustExist)
{
    FILE *fptr;

    if ((fptr = fopen(name,mode)) == NULL && mustExist){
        printf("Error! opening file");
        exit(1);
    }

    return fptr;
}

void closeFileHandle( FILE * handle )
{
    // ASSERT NOT NULL
    fclose(handle);
}

void fileRead( void * buffer, long unsigned size, long unsigned count, FILE * handle )
{
    long unsigned r;
    r = fread(buffer, size, count, handle);
    
    if (r != count) {
        printf("Error! reading file %x, %x", r, count);
        exit(1);
    }
}

void fileWrite( void * buffer, long unsigned size, long unsigned count, FILE * handle )
{
    long unsigned r;
    r = fwrite(buffer, size, count, handle);

    if (r!=count) {
        printf("Error! writing file");
        exit(1);
    }
}


void changeExtension( char * name, const char * newext)
{
    char * token;
    token = strtok(name, ".");

    if (token == NULL) {
        printf("Error! invalid file name for extension change");
        exit(1);      
    }

    strcat(name, ".");
    strcat(name, newext);
}