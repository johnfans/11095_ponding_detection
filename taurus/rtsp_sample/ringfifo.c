#include<stdio.h>
#include<ctype.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "ringfifo.h"
#include "rtputils.h"
#include "sample_comm.h"
#define NMAX 64

int iput = 0;
int iget = 0;
int n = 0;

struct ringbuf ringfifo[NMAX];
extern int UpdateSpsOrPps(unsigned char *data,int frame_type,int len);

void ringmalloc(int size)
{
    int i;
    for(i =0; i<NMAX; i++)
    {
        ringfifo[i].buffer = malloc(size);
        ringfifo[i].size = 0;
        ringfifo[i].frame_type = 0;
    }
    iput = 0;
    iget = 0;
    n = 0;
}

void ringreset()
{
    iput = 0;
    iget = 0;
    n = 0;
}

void ringfree(void)
{
    int i;
    printf("begin free mem\n");
    for(i =0; i<NMAX; i++) {
        free(ringfifo[i].buffer);
        ringfifo[i].size = 0;
    }
}

int addring(int i)
{
    return (i+1) == NMAX ? 0 : i+1;
}

int ringget(struct ringbuf *getinfo)
{
    int Pos;
    if(n>0) {
        Pos = iget;
        iget = addring(iget);
        n--;
        getinfo->buffer = (ringfifo[Pos].buffer);
        getinfo->frame_type = ringfifo[Pos].frame_type;
        getinfo->size = ringfifo[Pos].size;
        //getinfo->timesnap = ringfifo[Pos].timesnap;
        //pr
        return ringfifo[Pos].size;
    } else {
        //printf("Buffer is empty\n");
        return 0;
    }
}

void ringput(unsigned char *buffer,int size,int encode_type)
{
    if (n<NMAX) {
        memcpy(ringfifo[iput].buffer,buffer,size);
        ringfifo[iput].size= size;
        ringfifo[iput].frame_type = encode_type;
        //printf("Put FIFO INFO:idx:%d,len:%d,ptr:%x,type:%d\n",iput,ringfifo[iput].size,(int)(ringfifo[iput].buffer),ringfifo[iput].frame_type);
        iput = addring(iput);
        n++;
    } else {
        //  printf("Buffer is full\n");
    }
}

HI_S32 HisiPutH264DataToBuffer(VENC_STREAM_S *pstStream)
{
    HI_S32 i,j;
    HI_S32 len=0,off=0,len2=2;
    unsigned char *pstr;
    int iframe=0;
    for (i = 0; i < pstStream->u32PackCount; i++) {
        len+=pstStream->pstPack[i].u32Len-pstStream->pstPack[i].u32Offset;
    }
    if (n<NMAX) {
        for (i = 0; i < pstStream->u32PackCount; i++) {
            memcpy(ringfifo[iput].buffer+off,pstStream->pstPack[i].pu8Addr+pstStream->pstPack[i].u32Offset,pstStream->pstPack[i].u32Len-pstStream->pstPack[i].u32Offset);
            off+=pstStream->pstPack[i].u32Len-pstStream->pstPack[i].u32Offset;
            pstr=pstStream->pstPack[i].pu8Addr+pstStream->pstPack[i].u32Offset;
            if(pstr[4]==0x67) {
                UpdateSps(ringfifo[iput].buffer+off,9);
                iframe=1;
            } if(pstr[4]==0x68) {
                UpdatePps(ringfifo[iput].buffer+off,4);
            }
        }
        ringfifo[iput].size= len;
        if (iframe) {
            ringfifo[iput].frame_type = FRAME_TYPE_I;
        } else {
            ringfifo[iput].frame_type = FRAME_TYPE_P;
        }
        iput = addring(iput);
        n++;
    }
    return HI_SUCCESS;
}
