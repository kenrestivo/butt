// string manipulation functions for butt
//
// Copyright 2007-2008 by Daniel Noethen.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or (at your option)
// any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int strinsrt(char **dest, char *insert, char *pos)
{
    char *pre;
    char *post;
    char *temp;
    int pre_len;
    int post_len;
    int new_len;

    new_len = strlen(*dest) + strlen(insert);
    pre_len = strlen(*dest) - strlen(pos);
    post_len = strlen(pos);

    pre = (char*)malloc(pre_len*sizeof(char) +1);
    post = (char*)malloc(post_len*sizeof(char) +1);
    temp = (char*)malloc(new_len*sizeof(char) +1);

    memcpy(pre, *dest, pre_len);
    pre[pre_len] = '\0';

    memcpy(post, *dest+pre_len, post_len);
    post[post_len] = '\0';

    sprintf(temp, "%s%s%s", pre, insert, post);

    *dest = (char*)realloc(*dest, new_len*sizeof(char) +1);
    strcpy(*dest, temp);
    
    return 0;
}

//returns a pointer to the last occurance of string needle in string haystack
char *strrstr(char *haystack, char *needle)
{
    char *last;
    char *found = NULL;

    do 
    {
        last = found;
        found = strstr(haystack, needle);

        if(found != NULL)
            haystack = found+strlen(needle);

    }while(found != NULL);

    return last;
}

//replaces all strings named by search with strings named by replace in dest
int strrpl(char **dest, char *search, char *replace)
{
    char *loc;
    char *temp;
    char *orig;
    char *result;
    int search_len, repl_len, diff;
    int size;
    int count;

    search_len = strlen(search);
    repl_len = strlen(replace);
    diff = repl_len - search_len;

    //how many strings do we need to replace?
    temp = *dest;
    for(count = 0; (temp = strstr(temp, search)); count++)
        temp += search_len;

    //length of the new string
    size = strlen(*dest) + (diff*count);

    temp = strdup(*dest);
    orig = temp;

    result = (char*)malloc(size*sizeof(char) +1);
    if(!result)
        return -1;

    memset(result, 0, size);

    //build the new string
    while((loc = strstr(temp, search))) 
    {
        strncat(result, temp, loc - temp);
        strcat(result, replace);
        temp = loc + strlen(search);
    }
    //append the rest (if any)
    if(strlen(temp) > 0)
        strcat(result, temp);

    *dest = (char *)realloc(*dest, size*sizeof(char) +1);
    if(!*dest)
        return -1;

    //save the new string back to orig position in memory
    strcpy(*dest, result);

    free(orig);
    free(result);

    return 0;
}

