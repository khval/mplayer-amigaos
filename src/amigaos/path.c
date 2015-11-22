#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mp_msg.h"
#include "path.h"

static const char * const homedir = "progdir:";
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

const char *mp_basename(const char *path)
{
    char *s;

    // check to find end of volume.
    s = strrchr(path, ':');
    if (s) path = s + 1;

    // check to find end of path
    s = strrchr(path, '/');
    return s ? s + 1 : path;
}

/**
 * @brief Allocates a new buffer containing the directory name
 * @param path Original path. Must be a valid string.
 *
 * @note The path returned always contains a trailing slash '/'.
 *       On systems supporting DOS paths, '\' is also considered as a directory
 *       separator in addition to the '/'.
 */
char *mp_dirname(const char *path)
{
    const char *base = mp_basename(path);
    size_t len = base - path;
    char *dirname;

    if (len == 0)
        return strdup("");
    dirname = malloc(len + 1);
    if (!dirname)
        return NULL;
    strncpy(dirname, path, len);
    dirname[len] = '\0';
    return dirname;
}

/**
 * @brief Join two paths if path is not absolute.
 * @param base File or directory base path.
 * @param path Path to concatenate with the base.
 * @return New allocated string with the path, or NULL in case of error.
 * @warning Do not forget the trailing path separator at the end of the base
 *          path if it is a directory: since file paths are also supported,
 *          this separator will make the distinction.
 * @note Paths of the form c:foo, /foo or \foo will still depends on the
 *       current directory on Windows systems, even though they are considered
 *       as absolute paths in this function.
 */
char *mp_path_join(const char *base, const char *path)
{
	char *tmp,*ret = NULL;
	int l;

	if (path)
	{
		l= strlen(path);
		if ((l>0) && path[l-1] == ':')	// if path is volume.
			return strdup(path);
	}

	ret = mp_dirname(base);
	if (!ret) return NULL;

	tmp = realloc(ret, strlen(ret) + strlen(path) + 1);
	if (!tmp) {
		free(ret);
		return NULL;
	}

	ret = tmp;
	strcat(ret, path);
    
	return ret;
}


/**
 * @brief Same as mp_path_join but always treat the first parameter as a
 *        directory.
 * @param dir Directory base path.
 * @param append Right part to append to dir.
 * @return New allocated string with the path, or NULL in case of error.
 */
char *mp_dir_join(const char *dir, const char *append)
{
	char *ret = NULL;
	int dirlen = 0;
	int i = 0;
	if (dir) dirlen = strlen(dir);
	if (dirlen) i = dirlen -1;

	if (i>0)				
	{
		if (dir[i] == ':' || dir == '/')
		{
			if (ret = malloc( strlen(dir) + strlen(append) +1 ))
				sprintf(ret,"%s%s",dir,append);
		}
		else
		{
			if (ret = malloc( strlen(dir) + strlen(append) +2 ))		
				sprintf(ret,"%s/%s",dir,append);
		}
	}
	else
	{
		return strdup(append ? append : "");
	}
	return ret;
}
