long rxid_get_sub_visibility() { return mpctx->sh_video?sub_visibility:0; }
long rxid_get_percent_pos() { return demuxer_get_percent_pos(mpctx->demuxer); }
long rxid_get_time_length() { return (double)(1000.0*demuxer_get_time_length(mpctx->demuxer)); }
long rxid_get_vo_fullscreen() { return (mpctx->video_out && vo_config_count)?vo_fs:0; }

long rxid_get_time_pos()
{
  if(mpctx->sh_video)
    return (long)(1000.0*mpctx->sh_video->pts);
  else if(mpctx->sh_audio && mpctx->audio_out)
    return (long)(1000.0 * (mpctx->sh_audio->stream_delay - mpctx->audio_out->get_delay() * playback_speed));
  else
    return 0;
}

