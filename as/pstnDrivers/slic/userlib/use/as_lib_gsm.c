/*
 * $Author: lizhijie $
 * $Log: as_lib_gsm.c,v $
 * Revision 1.1.1.1  2006/11/29 09:16:55  lizhijie
 * AS600 Drivers
 *
 * Revision 1.1.1.1  2006/07/13 04:21:32  lizhijie
 * rebuild
 *
 * Revision 1.1.1.1  2005/03/14 06:56:04  lizhijie
 * new drivers for all devices
 *
 * Revision 1.3  2004/12/17 07:36:51  lizhijie
 * add debug for IXP422, such as Hardware gain, DMA buffer size, software Gain
 *
 * Revision 1.2  2004/12/15 07:33:05  lizhijie
 * recommit
 *
 * Revision 1.1.1.1  2004/11/18 07:00:04  lizhijie
 * driver for assist telephone cards Tiger320-Si3210/3050
 *
 * $Revision: 1.1.1.1 $
*/
#include <fcntl.h>
#include <sys/ioctl.h>
#include "as_gsm.h"

#include "as_dev_ctl.h"
#include "assist_lib.h"

#define AS_GSM_DEBUG	1

char gsm_silence[] = /* 33 */
{0xD8,0x20,0xA2,0xE1,0x5A,0x50,0x00,0x49,0x24,0x92,0x49,0x24,0x50,0x00,0x49
,0x24,0x92,0x49,0x24,0x50,0x00,0x49,0x24,0x92,0x49,0x24,0x50,0x00,0x49,0x24
,0x92,0x49,0x24};


//unsigned char gsm[66];				/* Two Real GSM Frames */


static as_gsm_engine_t  _gsm_engine =
{
	_gsm : 	NULL,
	inited :	0	
};

static void as_gsm_init()
{
	if(! _gsm_engine.inited)
	{
		_gsm_engine._gsm = gsm_create();
#if AS_GSM_DEBUG		

#endif
		_gsm_engine.inited = 1;
	}
}

static void as_gsm_destroy()
{
	if(_gsm_engine.inited)
	{
		if(_gsm_engine._gsm)	
		{
#if AS_GSM_DEBUG		

#endif
			gsm_destroy(_gsm_engine._gsm);
		}

		_gsm_engine.inited = 0;
	}
}

as_gsm_data_frame_t  *as_gsm_read(int fd )
{
	int res;
	as_gsm_frame_t gsm_frame;
	as_gsm_data_frame_t *gsm_data_frame;

	printf("%s.%d\r\n", __FUNCTION__, __LINE__);
	if ((res = read(fd, &gsm_frame.data, AS_GSM_FRAME_LENGTH)) != AS_GSM_FRAME_LENGTH ) 
	{
		if (res)
			printf( "Short read (%d) (%s)!\n", res, strerror(errno));
		return NULL;
	}

	printf("%s.%d\r\n", __FUNCTION__, __LINE__);
	gsm_data_frame = (as_gsm_data_frame_t *)malloc(sizeof(as_gsm_data_frame_t ));
	if(gsm_data_frame==NULL)
	{
		printf("Memory allocate fail!\r\n");
		exit(1);
	}
	
	printf("%s.%d\r\n", __FUNCTION__, __LINE__);
	res = gsm_decode(_gsm_engine._gsm, gsm_frame.data, gsm_data_frame->data);
	if(res)
	{
		printf("GSM decode failed\r\n");
		return NULL;
	}
	
	printf("%s.%d\r\n", __FUNCTION__, __LINE__);
	return gsm_data_frame;
}


#if 0
static long gsm_tell(struct ast_filestream *fs)
{
	off_t offset;
	offset = lseek(fs->fd, 0, SEEK_CUR);
	return (offset/33)*160;
}

static int gsm_seek(struct ast_filestream *fs, long sample_offset, int whence)
{
	off_t offset=0,min,cur,max,distance;
	
	min = 0;
	cur = lseek(fs->fd, 0, SEEK_CUR);
	max = lseek(fs->fd, 0, SEEK_END);
	/* have to fudge to frame here, so not fully to sample */
	distance = (sample_offset/160) * 33;
	if(whence == SEEK_SET)
		offset = distance;
	else if(whence == SEEK_CUR || whence == SEEK_FORCECUR)
		offset = distance + cur;
	else if(whence == SEEK_END)
		offset = max - distance;
	// Always protect against seeking past the begining.
	offset = (offset < min)?min:offset;
	if (whence != SEEK_FORCECUR) {
		offset = (offset > max)?max:offset;
	} else if (offset > max) {
		int i;
		lseek(fs->fd, 0, SEEK_END);
		for (i=0; i< (offset - max) / 33; i++) {
			write(fs->fd, gsm_silence, 33);
		}
	}
	return lseek(fs->fd, offset, SEEK_SET);
}
#endif

void swap(unsigned short *buf, int length)
{
	unsigned char a, b;
	int i;
	for(i=0; i<length; i++)
	{
		a = (*(buf+i))>>8;
		b = *(buf+i);
		*(buf+i) = (b<<8)|a;
	}
}

void as_gsm_file_play(char *gsmfile, int devfd)
{
	int gsmfd;
	int linear = 1 ;
	int i=0;
	int length;
	as_gsm_data_frame_t *frame;
	
	if(!_gsm_engine.inited)
		as_gsm_init();

#if 1	
	ioctl( devfd, AS_CTL_SETLINEAR, &linear);
#endif

	gsmfd =open(gsmfile/*"agent-alreadyon.gsm"*/, O_RDWR);
	if (gsmfd < 0) 
	{
		fprintf(stderr, "Unable to open agent-alreadyon.gsm: %s\n",  strerror(errno));
		exit(1);
	}

	frame = as_gsm_read(gsmfd);
#if AS_GSM_DEBUG
	printf("Now Play audio in channel.....\r\n");
#endif
	printf("Now Play audio in channel.....\r\n");

	while( frame )
	{
		i++;
#if 0
		int j;
		unsigned char xlaw_buf[AS_GSM_DATA_LENGTH];
#endif

#if AS_GSM_DEBUG
		printf("%d frame ", i);
#endif

#if 1
//		swap(frame->data, AS_GSM_DATA_LENGTH);
		length = write(devfd, frame->data, AS_GSM_DATA_LENGTH*2);
		if(length!=AS_GSM_DATA_LENGTH*2)
			printf("Write length is not correct, it is %d, ought to be %d\r\n" , length, AS_GSM_DATA_LENGTH*2);
#endif

#if 0
		for(j=0; j< AS_GSM_DATA_LENGTH; j++)
		{
			xlaw_buf[j] = FULLXLAW(*(frame->data+j), U_LAW_CODE);
		}
		write(devfd, xlaw_buf, AS_GSM_DATA_LENGTH);
#endif		
		free(frame);
		frame = as_gsm_read(gsmfd);
	}	
#if AS_GSM_DEBUG
	printf("\r\nPlay audio over:total %d frames\r\n", i);
#endif
	close(gsmfd);
#if 1
	linear = 0;
	ioctl( devfd, AS_CTL_SETLINEAR, &linear);
#endif
	as_gsm_destroy();

}

