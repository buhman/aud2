#include <stdio.h>
#include <assert.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>

#include <alsa/asoundlib.h>

void
enprintf(int err, const char *fmt, ...)
{
  if (!(err < 0)) return;

  va_list ap;

  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);

  exit(1);
}

void
eaprintf(int err, const char *fmt, ...)
{
  va_list ap;
  char buf[1024];

  if (!(err < 0)) return;

  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);

  av_strerror(err, buf, sizeof (buf));
  fprintf(stderr, ": %s\n", buf);

  exit(1);
}

void
esprintf(int err, const char *fmt, ...)
{
  va_list ap;

  if (!(err < 0)) return;

  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);

  fprintf(stderr, ": %s\n", snd_strerror(err));

  exit(1);
}

static int
get_snd_format_from_av_format(enum AVSampleFormat av_format,
                              snd_pcm_format_t *snd_format)
{
  size_t i;
  struct sample_format_entry {
    enum AVSampleFormat av_format;
    snd_pcm_format_t snd_format_le;
    snd_pcm_format_t snd_format_be;
  } sample_format_table[] = {
    { AV_SAMPLE_FMT_U8,   SND_PCM_FORMAT_U8,          SND_PCM_FORMAT_U8         },
    { AV_SAMPLE_FMT_S16,  SND_PCM_FORMAT_S16_LE,      SND_PCM_FORMAT_S16_BE     },
    { AV_SAMPLE_FMT_S32,  SND_PCM_FORMAT_S32_LE,      SND_PCM_FORMAT_S32_BE     },
    { AV_SAMPLE_FMT_FLT,  SND_PCM_FORMAT_FLOAT_LE,    SND_PCM_FORMAT_FLOAT_BE   },
    { AV_SAMPLE_FMT_DBL,  SND_PCM_FORMAT_FLOAT64_LE,  SND_PCM_FORMAT_FLOAT64_BE },
  };
  struct sample_format_entry *entry;

  for (i = 0; i < (sizeof (sample_format_table) / sizeof (struct sample_format_entry)); i++) {
    entry = &sample_format_table[i];
    if (av_format == entry->av_format) {
      *snd_format = AV_NE(entry->snd_format_be, entry->snd_format_le);
      return 0;
    }
  }

  return -1;
}

int
decode(AVCodecContext *codec_context,
       AVPacket *packet,
       AVFrame *frame,
       SwrContext *swr,
       snd_pcm_t *handle)
{
  uint8_t **output;
  int ret;
  int read_samples = 0;

  output = NULL;

  ret = avcodec_send_packet(codec_context, packet);
  eaprintf(ret, "avcodec_send_packet");

  while (1) {
    ret = avcodec_receive_frame(codec_context, frame);
    if (packet == NULL) {
      char buf[1024];
      av_strerror(ret, buf, sizeof (buf));
      fprintf(stderr, "%s\n", buf);
    }
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
      break;
    else
      eaprintf(ret, "avcodec_recieve_frame");

    //

    av_samples_alloc_array_and_samples(&output, NULL, frame->channels, frame->nb_samples, frame->format, 0);
    ret = swr_convert(swr, output, frame->nb_samples, (const uint8_t **)frame->data, frame->nb_samples);
    esprintf(ret, "swr_convert");

    //

    {
      snd_pcm_sframes_t samples;
      samples = snd_pcm_writei(handle, *output, frame->nb_samples);
      if (samples < 0)
        samples = snd_pcm_recover(handle, samples, 0);
      esprintf(samples, "snd_pcm_writei: %d", samples);

      if (samples > 0 && samples < frame->nb_samples)
        fprintf(stderr, "Short write (expected %i, wrote %li)\n", frame->nb_samples, samples);

      //

      read_samples += frame->nb_samples;
    }

    //

    av_freep(output);
  }

  return read_samples;
}

int
play(const char *filename)
{
  AVFormatContext *format_context = NULL;
  AVCodec *codec = NULL;
  AVCodecContext *codec_context = NULL;
  int stream_index;
  int ret;

  ret = avformat_open_input(&format_context, filename, NULL, NULL);
  eaprintf(ret, "avformat_open_input: %s", filename);

  assert(avformat_find_stream_info(format_context, NULL) == 0);

  ret = av_find_best_stream(format_context, AVMEDIA_TYPE_AUDIO, -1, -1, &codec, 0);
  eaprintf(ret, "av_find_best_stream");

  stream_index = ret;
  fprintf(stderr, "selected stream: %i\n", stream_index);

  codec_context = avcodec_alloc_context3(codec);
  assert(codec_context);

  ret = avcodec_parameters_to_context(codec_context, format_context->streams[stream_index]->codecpar);
  eaprintf(ret, "avcodec_parameters_to_context");

  fprintf(stderr, "stream[%i]: codec=%s\n", stream_index, codec->name);

  ret = avcodec_open2(codec_context, codec, NULL);
  eaprintf(ret, "avcodec_open2");

  enum AVSampleFormat av_format;
  av_format = av_get_packed_sample_fmt(codec_context->sample_fmt);

  snd_pcm_format_t snd_format;
  ret = get_snd_format_from_av_format(av_format, &snd_format);
  enprintf(ret, "get_snd_format_from_av_format: %d %s\n", codec_context->sample_fmt, av_get_sample_fmt_name(codec_context->sample_fmt));

  snd_pcm_t *handle;
  ret = snd_pcm_open(&handle, "default", SND_PCM_STREAM_PLAYBACK, 0);
  esprintf(ret, "snd_pcm_open: default");

  fprintf(stderr,
          "samples: format=%s rate=%i channels=%i\n",
          av_get_sample_fmt_name(codec_context->sample_fmt),
          codec_context->sample_rate,
          codec_context->channels);

  ret = snd_pcm_set_params(handle,
                           snd_format,
                           // type pulse does not appear to support noninterleaved
                           SND_PCM_ACCESS_RW_INTERLEAVED,
                           codec_context->channels,
                           codec_context->sample_rate,
                           0,
                           500000);
  esprintf(ret, "snd_pcm_set_params");


  //

  SwrContext *swr;
  swr = swr_alloc_set_opts(NULL,
                           // out
                           codec_context->channel_layout,
                           av_get_packed_sample_fmt(codec_context->sample_fmt),
                           codec_context->sample_rate,
                           // in
                           codec_context->channel_layout,
                           codec_context->sample_fmt,
                           codec_context->sample_rate,
                           // log
                           0,
                           NULL);
  ret = swr_init(swr);
  esprintf(ret, "swr_init");


  //

  AVPacket *packet; // demultiplexed, encoded
  packet = av_packet_alloc();
  AVFrame *frame;
  frame = av_frame_alloc();

  /*
  ret = snd_pcm_wait(handle, -1);
  printf("snd_pcm_wait: %d\n", ret);
  ret = snd_pcm_start(handle);
  printf("snd_pcm_start: %d\n", ret);
  */

  int read_samples = 0;

  av_init_packet(packet);

  while (av_read_frame(format_context, packet) >= 0) {
    if (packet->stream_index != stream_index)
      continue;

    ret = decode(codec_context, packet, frame, swr, handle);

    read_samples += ret;
    if (read_samples > codec_context->sample_rate) {
      read_samples -= codec_context->sample_rate;
      fprintf(stderr, ".");
    }

    av_packet_unref(packet);
    av_init_packet(packet);
  }

  fprintf(stderr, "\n");
  decode(codec_context, NULL, frame, swr, handle);

  snd_pcm_close(handle);
  snd_config_update_free_global();
  swr_free(&swr);
  av_packet_free(&packet);
  av_frame_free(&frame);
  avcodec_free_context(&codec_context);
  avformat_close_input(&format_context);

  return 0;
}

int
main(int argc, char *argv[])
{
  int i;
  for (i = 1; i < argc; i++) {
    fprintf(stderr, "%s\n", argv[i]);
    play(argv[i]);
  }

  return 0;
}
