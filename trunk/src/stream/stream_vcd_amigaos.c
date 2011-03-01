
#include "config.h"

#ifdef CONFIG_VCD
#include <unistd.h>
#include "mp_msg.h"
#include "stream.h"
#include "help_mp.h"
#include "../m_option.h"
#include "../m_struct.h"

#include <fcntl.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <errno.h>

#include "vcd_read_amiga.h"

extern char *cdrom_device;

static struct stream_priv_s {
  int track;
  char* device;
} stream_priv_dflts = {
  1,
  NULL
};

#define ST_OFF(f) M_ST_OFF(struct stream_priv_s,f)
/// URL definition
static m_option_t stream_opts_fields[] = {
  { "track", ST_OFF(track), CONF_TYPE_INT, M_OPT_MIN, 1, 0, NULL },
  { "device", ST_OFF(device), CONF_TYPE_STRING, 0, 0 ,0, NULL},
  /// For url parsing
  { "hostname", ST_OFF(track), CONF_TYPE_INT, M_OPT_MIN, 1, 0, NULL },
  { "filename", ST_OFF(device), CONF_TYPE_STRING, 0, 0 ,0, NULL},
  { NULL, NULL, 0, 0, 0, 0,  NULL }
};
static struct m_struct_st stream_opts = {
  "vcd",
  sizeof(struct stream_priv_s),
  &stream_priv_dflts,
  stream_opts_fields
};

static int fill_buffer(stream_t *s, char* buffer, int max_len){
  return vcd_read(s->priv,buffer);
}

static int seek(stream_t *s,off_t newpos) {
  s->pos = newpos;
  vcd_set_msf(s->priv,s->pos/VCD_SECTOR_DATA);
  return 1;
}

static void close_s(stream_t *stream) {
  free(stream->priv);
}

static int open_s(stream_t *stream,int mode, void* opts, int* file_format) {
  struct stream_priv_s* p = (struct stream_priv_s*)opts;
  int ret,ret2;
  struct mp_vcd_priv_t* vcd;

  if(mode != STREAM_READ) {
    m_struct_free(&stream_opts,opts);
    return STREAM_UNSUPPORTED;
  }

  if (!p->device) {
    if(cdrom_device)
      p->device = strdup(cdrom_device);
    else
      p->device = strdup(DEFAULT_CDROM_DEVICE);
  }

	if ( ! (vcd = vcd_open(p->device) ) )
	{
    mp_msg(MSGT_OPEN,MSGL_ERR,MSGTR_CdDevNotfound,p->device);
    vcd_close(vcd);
    m_struct_free(&stream_opts,opts);
    return STREAM_ERROR;
  }

  if(vcd_read_toc(vcd) < 0)
	{
    mp_msg(MSGT_OPEN,MSGL_ERR,"Failed to get cd toc\n");
    vcd_close(vcd);
    m_struct_free(&stream_opts,opts);
    return STREAM_ERROR;
  }
  ret2=vcd_get_track_end(vcd,p->track);
  if(ret2<0){
    mp_msg(MSGT_OPEN,MSGL_ERR,MSGTR_ErrTrackSelect " (get)\n");
    vcd_close(vcd);
    m_struct_free(&stream_opts,opts);
    return STREAM_ERROR;
  }
  ret=vcd_seek_to_track(vcd,p->track);
  if(ret<0){
    mp_msg(MSGT_OPEN,MSGL_ERR,MSGTR_ErrTrackSelect " (seek)\n");
    vcd_close(vcd);
    m_struct_free(&stream_opts,opts);
    return STREAM_ERROR;
  }
  mp_msg(MSGT_OPEN,MSGL_V,"VCD start byte position: 0x%X  end: 0x%X\n",ret,ret2);

  stream->fd = 0;			// Useless
  stream->type = STREAMTYPE_VCD;
  stream->sector_size = VCD_SECTOR_DATA;
  stream->start_pos=ret;
  stream->end_pos=ret2;
  stream->priv = vcd;

  stream->fill_buffer = fill_buffer;
  stream->seek = seek;
  stream->close = close_s;

  m_struct_free(&stream_opts,opts);
  return STREAM_OK;
}

stream_info_t stream_info_vcd = {
  "Video CD",
  "vcd",
  "DET Nicolas",
  "based on stream_vcd (a lot !)",
  open_s,
  { "vcd", NULL },
  &stream_opts,
  1 // Urls are an option string
};

#endif
