/**
 * Copyright (c) 2003 Aleander Belov <asbel@mail.ru>.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
 
/* For saving configuration in file use function 
   int config_save(const char *INIT_name, const char *INIT_val, const int INIT_num).
   
   INIT_name - Parameter name
   INIT_val - Parameter value
   INIT_num - number of parameter (f.e. for key_quit)
   
   look at the end of file for example and change 
   #define CFGFILE "/home/asbel/.tvtime/tvtimerc".
   for compile use gcc configsave.c && ./a.out
*/

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define BUFSIZE 80
#define _strncpy(A,B,C) strncpy(A,B,C), *(A+(C)-1)='\0'

static int FD = -1;

/* small wrapper for fgets(3) */
static char *_fgets(int F)
{
    char arr[BUFSIZE];
    char *ptr1, *ptr2, *str = arr;
    int i;
    ssize_t cnt;

    for(i=0; (cnt=read(F, &arr[i], 1)) == 1 && arr[i] != '\n' && i<BUFSIZE; i++);
    if(cnt != 1 && !i) return NULL;
    if(arr[i] == '\n') arr[i+1] = '\0';
    else arr[i] = '\0';
    
    if (*(str+strlen(str)-1) != '\n')
    {
        ptr1=_fgets(F);
        if(ptr1 != NULL)
        {
            if((ptr2 = malloc(strlen(ptr1)+strlen(str)+1)) == NULL) return NULL;
            strcpy(ptr2, str);
            strcat(ptr2, ptr1);
            free(ptr1);
            return ptr2;
        }
    }
    
    /* Not +1!!! This need for error string in config file:
       Name = 
    */
    ptr2 = malloc(strlen(str)+2);
    if(ptr2 == NULL) return NULL;
    strcpy(ptr2, str);
    return ptr2;
}

int configsave_open(const char *filename)
{
    if( filename == NULL ||
	(FD=open(filename, O_RDWR|O_CREAT, S_IRUSR|S_IWUSR)) == -1 )
    {
	fprintf(stderr, "Can't open file %s for saving configuration: ", filename);
	perror("");
	return 0;
    }
    return 1;
}

void configsave_close(void)
{
    if( FD != -1 ) close(FD);
    FD = -1;
}

int configsave(const char *INIT_name, const char *INIT_val, const int INIT_num)
{
    char *str, *namend, *name, *val, *ptr, c;
    int num = 1;
    off_t offset = 0L, delta = 0L;
    
    lseek(FD, 0L, SEEK_SET);
    if( FD == -1 ||
	INIT_name == NULL ||
	INIT_val == NULL  ||
	INIT_num          < 1 ) return 0;
    
    while(1)
    {
	offset = lseek(FD, 0L, SEEK_CUR);
	if((str = _fgets(FD)) == NULL)
	{
	    offset=strlen("\n# Adding parameter \n = \n") + strlen(INIT_name)*2 + strlen(INIT_val)+1;
	    str = malloc(sizeof(char)*offset);
	    snprintf(str, offset,  "\n# Adding parameter %s\n%s = %s\n", INIT_name, INIT_name, INIT_val);
	    write(FD, str, offset-1);
	    free(str);
	    return 1;
	}

	/* Remove comments*/
	if((ptr=strchr(str, '#')) != NULL) *ptr = '\0';

	/* This line is not 'name = value' - skip it 
	   or name is not = INIT_name
	*/
	if((val=strchr(str, '=')) == NULL || val == str)
	{
	    free(str);
	    continue;
	}
	
	for(name=str; isspace(*name); name++);
	for(namend=val; isspace(*namend) || *namend=='='; namend--);
	c=*(++namend); *namend='\0';
	if(strcasecmp(name, INIT_name) != 0)
	{
	    free(str);
	    continue;
	}
	if(num == INIT_num) break;
	++num;
	free(str);
    }

/* Now we replacing the value in nv pair */
    *namend=c;
    if(ptr != NULL) *ptr = '#';
    else ptr = val+strlen(val)-1;

    while((*val == ' ' || *val == '\t' || *val == '=') && val<ptr) ++val;
    while(isspace(*(ptr-1)) && ptr>val) --ptr;

    if(isspace(*val) || *val == '#') delta=strlen(INIT_val);
    else
    {
	if(ptr == val) delta=strlen(INIT_val)-strlen(val);
	else delta=strlen(INIT_val)-(ptr-val);
    }
    
    if(delta > 0L)
    {
	off_t pos;
	char c;
	for(pos = lseek(FD, -1L, SEEK_END); pos>offset; pos--)
	{
	    num = read(FD, &c, 1);
	    lseek(FD, delta-1L, SEEK_CUR);
	    write(FD, &c, 1);
	    lseek(FD, pos, SEEK_SET);
	}
    }

    lseek(FD, offset, SEEK_SET);
    if(*val == '=') write(FD, str, val-str+1);
    else write(FD, str, val-str);
    write(FD, INIT_val, strlen(INIT_val));
    if(ptr != val) write(FD, ptr, strlen(ptr));
    free(str);

    if(delta < 0L)
    {
	char c;
	lseek(FD, -delta, SEEK_CUR);
	while(read(FD, &c, 1) != 0 )
	{
	    lseek(FD, delta-1L, SEEK_CUR);
	    write(FD, &c, 1);
	    lseek(FD, -delta, SEEK_CUR);
	}
	ftruncate(FD, lseek(FD, 0L, SEEK_CUR)+delta);
    }

    return 1;
}
