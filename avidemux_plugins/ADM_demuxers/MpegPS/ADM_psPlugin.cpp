/***************************************************************************
    copyright            : (C) 2007 by mean
    email                : fixounet@free.fr
    
      See lavformat/flv[dec/env].c for detail
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
#include "ADM_default.h"
#include "ADM_ps.h"
#include "ADM_demuxerInternal.h"
#include "fourcc.h"
#include "ADM_coreUtils.h"

ADM_DEMUXER_BEGIN( psHeader, 10,
                    1,0,0,
                    "ps",
                    "mpeg ps demuxer plugin (c) Mean 2007/2008"
                );

static bool detectPs(const char *file);
uint8_t   psIndexer(const char *file);
/**
    \fn Probe
*/

extern "C" ADM_PLUGIN_EXPORT uint32_t         probe(uint32_t magic, const char *fileName)
{
char *index=(char *)malloc(strlen(fileName)+6);
int count=0;
    if(!detectPs(fileName))
    {
        printf(" [PS Demuxer] Not a ps file\n");
	free(index);
        return false;
    }

    sprintf(index,"%s.idx2",fileName);
again:    
    if(ADM_fileExist(index)) 
    {
        printf(" [PS Demuxer] There is an index for that file \n");
        FILE *f=ADM_fopen(index,"rt");
        char signature[10];
        fread(signature,4,1,f);
        signature[4]=0;
        fclose(f);
        if(!strcmp(signature,"PSD1")) 
        {
              indexFile indexFile;
             char *type;
             if(!indexFile.open(index))
             {
                printf("[psDemux] Cannot open index file %s\n",index);
                indexFile.close();
                free(index);
                return false;
              }
             if(!indexFile.readSection("System"))
            {
                printf("[psDemux] Cannot read system section\n");
                indexFile.close();
                free(index);
                return false;
            }
            type=indexFile.getAsString("Type");
            if(!type || type[0]!='P')
                {
                    printf("[psDemux] Incorrect or not found type\n");
                    indexFile.close();
                    free(index);
                    return false;
                }
            free(index);
            return 50;
        }
        printf("[PsDemuxer] Not a valid index\n");
        return false;
    }
    if(count) 
    {
        free(index);
        return false;
    }
    printf("[PSDemuxer] Creating index..\n");
    count++;
    if(true==psIndexer(fileName)) goto again;
    printf("[PSDemuxer] Failed..\n");
    free(index);
   return 0;
}
#define PROBE_SIZE (1024*1024)
/**
    \fn detectPs
    \brief returns true if the file seems to be mpeg PS

*/
bool detectPs(const char *file)
{
    
    uint32_t bufferSize;
    uint32_t nbPacket,nbMatch=0;

    FILE *f=ADM_fopen(file,"rb");
    if(!f) return false;

    uint8_t *buffer = new uint8_t[PROBE_SIZE];
    bufferSize=fread(buffer,1,PROBE_SIZE,f);
    fclose(f);
    nbPacket=bufferSize/2300;
    uint8_t *head,*tail;
    head=buffer;
    tail=buffer+bufferSize;
    uint8_t code;
    uint32_t offset;
    bool r = false;
    // Is it a Seq Start ?
    if(!buffer[0] && !buffer[1] && buffer[2]==1 && buffer[3]==0xba)
    {
        printf("Starts with SEQUENCE_START, probably MpegPS\n");
        delete[] buffer;
        return true;
    }
    while(ADM_findMpegStartCode(head,tail,&code,&offset))
    {
        head+=offset;
        if(code==0xE0) nbMatch++;
    }
    printf(" match :%d / %d (probeSize:%d)\n",nbMatch,nbPacket,bufferSize);
    if (nbMatch * 10 > nbPacket * 2)
    {
        r = true;
    }
    delete[] buffer;
    return r;
}
//EOF
