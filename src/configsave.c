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
   int config_save(char *filename, char *INIT_name, char *INIT_val, int INIT_num).
   
   filename - Name of config file.
   INIT_name - Parameter name
   INIT_val - Parameter value
   INIT_num - number of parameter (f.e. for key_quit)
   
   look at the end of file for example and change 
   #define CFGFILE "/home/asbel/.tvtime/tvtimerc".
   for compile use gcc configsave.c && ./a.out
*/

#define CFGFILE "/home/asbel/.tvtime/tvtimerc"

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>

#define BUFSIZE 256

/* small wrapper for fgets(3) */
static char *_fgets(FILE *F)
{
    char arr[BUFSIZE];
    char *ptr1, *ptr2, *str = arr;

    if(fgets(str, BUFSIZE, F) == NULL) return NULL;

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
    ptr2 = malloc(strlen(str)+1);
    if(ptr2 == NULL) return NULL;
    strcpy(ptr2, str);
    return ptr2;
}


int config_save(char *filename, char *INIT_name, char *INIT_val, int INIT_num)
{
    FILE *F;
    char *str, *nstr, *name, *val, *ptr, c;
    int num = 1;
    long offset = 0, delta = 0;
    
    if( filename == NULL  || 
	INIT_name == NULL ||
	INIT_val == NULL  ||
	INIT_num          < 1 ) return 0;
    
    if((F=fopen(filename, "r+")) == NULL)
    {
	fprintf(stderr, "Can't open file %s for saving configuration: ", filename);
	perror("");
	return 0;
    }
    
    while(1)
    {
	offset = ftell(F);
	if((str = _fgets(F)) == NULL)
	{
	    fprintf(F, "\n# Adding parameter %s\n%s = %s\n", INIT_name, INIT_name, INIT_val);
	    fclose(F);
	    return 1;
	}

	/* Remove comments*/
	if((ptr=strchr(str, '#')) != NULL) *ptr = '\0';

	/* This line is not 'name = value' - skip it 
	   or name is not = INIT_name
	*/
	if((val=strchr(str, '=')) == NULL)
	{
	    free(str);
	    continue;
	}
	
	for(name=str; isspace(*name); name++);
	for(nstr=val; isspace(*nstr) || *nstr=='='; nstr--);
	c=*(++nstr); *nstr='\0';
	if(strcasecmp(name, INIT_name) != 0)
	{
	    free(str);
	    continue;
	}
	if(num == INIT_num) break;
	++num;
	free(str);
    }

    if(ptr != NULL) *ptr = '#';
    *nstr=c;

    /* Now we change the value */
    for(++val; isspace(*val); val++);
    for(ptr=val; !isspace(*ptr) && *ptr && *ptr != '#'; ptr++);

    delta=strlen(INIT_val)-(ptr-val+1)+1;
    *val='\0';

    if(delta > 0L)
    {
	long pos;
	fseek(F, -1L, SEEK_END);
	for(pos = ftell(F); pos>offset; pos--)
	{
	    num = fgetc(F);
	    fseek(F, delta-1, SEEK_CUR);
	    fputc(num, F);
	    fseek(F, pos, SEEK_SET);
	}
    }

    fseek(F, offset, SEEK_SET);
    fprintf(F, "%s%s%s", str, INIT_val, ptr);
    free(str);

    if(delta < 0L)
    {
	fseek(F, -delta, SEEK_CUR);
	while((num=fgetc(F)) != EOF)
	{
	    fseek(F, delta-1, SEEK_CUR);
	    fputc(num, F);
	    fseek(F, -delta, SEEK_CUR);
	}
	truncate(filename, ftell(F)+delta);
    }

    fclose(F);
    return 1;
}


int main(int argc, char **argv)
{
    config_save(CFGFILE, "freQuencies", "italy", 1);
    config_save(CFGFILE, "Frequencies", "europe-east", 1);
    config_save(CFGFILE, "frequeNcies", "us-cable", 1);
    config_save(CFGFILE, "frequencies", "france", 1);
    config_save(CFGFILE, "Freq", "russia", 1);
    config_save(CFGFILE, "key_quit", "qq", 2);
    config_save(CFGFILE, "key_quit", "qq", 3);
    return 1;
}
