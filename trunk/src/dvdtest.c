
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <proto/exec.h>
#include <proto/dos.h>

#define PATH_MAX 10000

typedef unsigned int uint8_t;

#include "libdvdcss/ioctl.h"
#include "libdvdcss/libdvdcss.h"
#include "libdvdcss/device.h"


static struct timezone dontcare = { 0,0 };
static struct timeval before, after;

void _print_error ( dvdcss_t dvdcss, char *txt )
{
	Printf("%s\n",txt);
}

int main()
{
	struct dvdcss_s private_dvdcss;
	char *buffer;
	int n;
	long long int read_bytes;
	long long int read_time;

	bzero(&private_dvdcss,sizeof(struct dvdcss_s) );

	if (buffer =  AllocVec( DVDCSS_BLOCK_SIZE , MEMF_SHARED | MEMF_CLEAR ))
	{
		private_dvdcss.psz_device = "sb600sata.device:1";

		if (_dvdcss_open(&private_dvdcss) == 0) 	// 0 = no error, -1 = error
		{
			gettimeofday(&before,&dontcare);	

			for (n=0;n<6000;n++)
			{
				private_dvdcss.i_pos = n ; 
				if (private_dvdcss.pf_read(&private_dvdcss,buffer,1) != 1)
				{
					printf("error reading data\n");
					break;
				}

				read_bytes += DVDCSS_BLOCK_SIZE;
				printf("progrss %d\r", n*100/6000);
			}

			gettimeofday(&after,&dontcare);

			read_time = (after.tv_usec - before.tv_usec) +(1000000 * (after.tv_sec - before.tv_sec));

			{
				char tbuffer[100];
				float kb = read_bytes / 1024;
				float secs = read_time / 1000 / 1000;

				sprintf(tbuffer,"read speed %0.00f KB/s, KB %d, secs %d ", (float)  (kb) / (float) (secs) , (int) kb, (int) secs );
				Printf("%s\n",tbuffer);
			}

			_dvdcss_close(&private_dvdcss);
		}
		FreeVec(buffer);
	}
}

