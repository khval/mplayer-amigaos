#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mp_msg.h"
#include "path.h"

static const char * const homedir = "/progdir/";
static const char * const config_dir = "conf";

char *codec_path = BINARY_CODECS_PATH;

char *get_path(const char *filename){
   char *buff;
	int len;

	len = strlen(homedir) + strlen(config_dir) + 1;
	if (filename == NULL) {
		if ((buff = (char *) malloc(len)) == NULL)
			return NULL;
		sprintf(buff, "%s%s", homedir, config_dir);
	} else {
		len += strlen(filename) + 1;
		if ((buff = (char *) malloc(len)) == NULL)
			return NULL;
		sprintf(buff, "%s%s/%s", homedir, config_dir, filename);
	}
	mp_msg(MSGT_GLOBAL,MSGL_V,"get_path('%s') -> '%s'\n",filename,buff);
	return buff;
}

static int needs_free = 0;

void set_codec_path(const char *path)
{
    if (needs_free)
        free(codec_path);
    if (path == 0) {
        codec_path = BINARY_CODECS_PATH;
        needs_free = 0;
        return;
    }
    codec_path = malloc(strlen(path) + 1);
    strcpy(codec_path, path);
    needs_free = 1;
}
