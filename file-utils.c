#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sfuzz.h"
#include "options-block.h"

void read_config(option_block *opts);
void file_error(char *msg, option_block *opts)
{
    fprintf(stderr, "[%s] error with file <%s:%d> : %s\n",
            get_time_as_log(), opts->pFilename, opts->lno, msg);
    exit(-1);
}


void add_symbol(char *sym_name, int sym_len, char *sym_val, int sym_val_len,
                option_block *opts, int i)
{
    sym_t *pSym;
    char buf[8192]= {0};
    char buf2[8192] = {0};

    opts->syms_array = realloc(opts->syms_array, 
                               sizeof(sym_t) * (opts->sym_count + 1));
    
    if(opts->syms_array == NULL)
    {
        file_error("out of memory adding symbol.", opts);
    }

    if(i == 0)
    {
	buf[0] = '%';
	memcpy(buf+1, sym_name, sym_len);
	
	snprintf(buf2, 8192, "%u", (unsigned int)strlen(sym_val));
	add_symbol(buf, strlen(buf), buf2, strlen(buf2), opts, 1);
	opts->syms_array = realloc(opts->syms_array, 
				   sizeof(sym_t) * (opts->sym_count + 1));
    
	if(opts->syms_array == NULL)
	{
	    file_error("out of memory adding symbol.", opts);
	}
    }
    opts->sym_count += 1;

    pSym = &(opts->syms_array[opts->sym_count - 1]);

    memset(pSym->sym_name, 0, 8192);
    memset(pSym->sym_val, 0, 8192);
    memcpy(pSym->sym_name, sym_name, sym_len);
    memcpy(pSym->sym_val,  sym_val,  sym_val_len);
}

void add_literal(option_block *opts, char *literal, int len)
{
    opts->litr = realloc(opts->litr, (opts->num_litr+1) * sizeof(char *));
    opts->litr_lens = realloc(opts->litr_lens, 
                              (opts->num_litr+1) * sizeof(int)); 
    if((opts->litr == NULL) || (opts->litr_lens == NULL))
    {
        file_error("too many literal strings - out of memory.", opts);
    }
    
    opts->litr[opts->num_litr] = malloc(len);
    if(opts->litr[opts->num_litr] == NULL)
    {
        file_error("literal too long - out of memory.", opts);
    }
    
    strncpy(opts->litr[opts->num_litr], literal, len);
    opts->litr_lens[opts->num_litr] = len;

    ++(opts->num_litr);
}

void add_sequence(option_block *opts, char *sequence, int len)
{
    opts->seq = realloc(opts->seq, (opts->num_seq+1) * sizeof(char *));
    opts->seq_lens = realloc(opts->seq_lens, 
                             (opts->num_seq+1) * sizeof(int)); 
    if((opts->seq == NULL) || (opts->seq_lens == NULL))
    {
        file_error("too many sequence strings - out of memory.", opts);
    }
    
    opts->seq[opts->num_seq] = malloc(len);
    if(opts->seq[opts->num_seq] == NULL)
    {
        file_error("sequence too long - out of memory.", opts);
    }
    
    strncpy(opts->seq[opts->num_seq], sequence, len);
    opts->seq_lens[opts->num_seq] = len;

    ++(opts->num_seq);
}

int readLine(option_block *opts, char *line, int len, int ign_cr)
{
    int size = 0;
    char c = 0;

    if(opts == NULL)
        file_error("null options reading line.", opts);
    
    if(opts->fp == NULL)
        file_error("empty file reading line.", opts);
    
    while((!feof(opts->fp)) && (len--))
    {
        size += fread(&c, 1, 1, opts->fp);
        *(line+(size - 1)) = c;
        if((c == '\n') || ((c == '\r') && (!ign_cr)))
            break;
    }
    line[size-1] = 0;
    return size;
}

int processFileLine(option_block *opts, char *line, int line_len)
{
    FILE *t;
    char *f;
    int state;
    int lno;

    char *delim;
    int   sze;
    switch(line[0])
    {
    case '/':
        if(line[1] != '/')
            break;
    case ';':
    case '#':
    case 0:
    case '\n':
        return 0;
    case '\r':
        if(line[1] == '\n')
            return 0;
        break;
    }

    if(opts->state == READ_MALFORM_BLOCK)
    {
        if(!strncmp("--", line, 2))
        {
            //end of malformed string.
            opts->state = CONFIG_PARSE_BEGIN;
            return 0;
        }
//        appendMalformedStr(opts, line);
        return 0;
    }

    /*not a comment, regular state*/
    
    if(!strncasecmp("literal", line, 7))
    {
        delim = strstr(line, "=");
        if(delim == NULL)
        {
            file_error("literal string not assigned!", opts);
        }
        sze = strlen(delim+1);
        if(sze == 0)
        {
            file_error("literal string is null!", opts);
        }
        add_literal(opts, delim+1, sze);
        return 0;
    }

    if(!strncasecmp("sequence", line, 7))
    {
        delim = strstr(line, "=");
        if(delim == NULL)
        {
            file_error("sequence string not assigned!", opts);
        }
        sze = strlen(delim+1);
        if(sze == 0)
        {
            file_error("sequence string is null!", opts);
        }
        add_sequence(opts, delim+1, sze);
        return 0;
    }
    
    if(!strncasecmp("reqwait", line, 7))
    {
        delim = strstr(line, "=");
        if(delim == NULL)
            file_error("request wait string not assigned!", opts);
        sze = strlen(delim+1);
        if(sze == 0)
            file_error("request wait string is null!", opts);
        opts->reqw_inms = atoi(delim+1);
        return 0;
    }

#if CONFIG_USE_MALFORMED_BLOCK_FMT
    if(!strncasecmp("malform-block", line, 13))
    {
        opts->state = READ_MALFORM_BLOCK;
        return 0;
    }
#endif

    if(!strncasecmp("maxseqlen", line, 9))
    {
        delim = strstr(line, "=");
        if(delim == NULL)
            file_error("max seq len not assigned!", opts);
        sze = strlen(delim+1);
        if(sze == 0)
            file_error("max seq len is null!", opts);
        opts->mseql = atoi(delim+1);
        return 0;
    }

    if(!strncasecmp("seqstep", line, 7))
    {
        delim = strstr(line, "=");
        if(delim == NULL)
            file_error("seq step not assigned!", opts);
        sze = strlen(delim+1);
        if(sze == 0)
            file_error("seq step is null!", opts);
        opts->seqstep = atoi(delim+1);
        return 0;
    }

    if(!strncasecmp("endcfg", line, 6))
        return 1;

    if(!strncasecmp("include", line, 7))
    {
        delim = strstr(line, " ");
        if(delim == NULL)
            file_error("include not assigned!", opts);
        sze = strlen(delim+1);
        if(sze == 0)
            file_error("include is null!", opts);
        
        t = opts->fp;
        f = malloc(strlen(opts->pFilename));
        if(f == NULL)
        {
            file_error("unable to include file - out of memory.", opts);
        }
        
        /*yeah yeah...not safe. So fuzz it, already!*/
        strcpy(f, opts->pFilename);
        state = opts->state;
        
        strncpy(opts->pFilename, delim+1, MAX_FILENAME_SIZE-1);
        
        /*setup for inner parse.*/
        opts->state = INIT_READ;
        lno = opts->lno;

        /*do inner parse.*/
        read_config(opts);
        
        strcpy(opts->pFilename, f);
        opts->state = state;
        opts->lno   = lno;
        opts->fp    = t;
        
        free(f);
        return 0;
    }

    if(line[0] == '$')
    {
        delim = strstr(line+1, "=");
        if(delim == NULL)
        {
            file_error("symbol not assigned!", opts);
        }
        sze = strlen(delim+1);
        if(sze == 0)
        {
            file_error("symbol is null!", opts);
        }
        add_symbol(line+1, (delim - (line+1)), delim+1, sze, opts, 0);
        return 0;
    }

    file_error("invalid config file.", opts);
    return 1;
}

void read_config(option_block *opts)
{
    char  done = 0;
    int   len  = 0;
    FILE *f;
    
    char line[8192]; // should never have more than an 8k line.

    if(opts->state != INIT_READ)
        file_error("invalid state for config reading.", opts);

    f = fopen(opts->pFilename, "r");

    if(f == NULL)
        file_error("unable to open file.", opts);

    opts->state = CONFIG_PARSE_BEGIN;
    opts->fp    = f;
    opts->lno   = 1;
    do
    {

        if((len = readLine(opts, line, 8192, 0)) == 0)
            done = 1;
        else
            done = processFileLine(opts, line, len);
        ++(opts->lno);
    }while(!done);

    if(opts->state != CONFIG_PARSE_BEGIN)
    {
        file_error("config file malformed!", opts);
    }

    opts->state = CONFIG_PARSE_END;
    return;
}
