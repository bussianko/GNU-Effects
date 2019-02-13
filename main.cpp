#include <iostream>

using namespace std;

extern "C"
{
    #include <libavcodec/avcodec.h>
    #include <libavformat/avformat.h>
    #include <libswscale/swscale.h>
    #include <libswscale/version.h>
}

void SaveFrame(AVFrame *pFrame, int width, int height, int iFrame) {
  FILE *pFile;
  char szFilename[32];
  int  y;

  // Open file
  sprintf(szFilename, "frame%d.ppm", iFrame);
  pFile=fopen(szFilename, "wb");
  if(pFile==NULL)
    return;

  // Write header
  fprintf(pFile, "P6\n%d %d\n255\n", width, height);

  // Write pixel data
  for(y=0; y<height; y++)
    fwrite(pFrame->data[0]+y*pFrame->linesize[0], 1, width*3, pFile);

  // Close file
  fclose(pFile);
}

int main(int argc, const char * argv[])
{
    av_register_all();
    AVFormatContext * FormatCtx = nullptr;
    if (argc < 2){
        cout << "Enter Full file path." << endl;
        return -1;
    }
    if (avformat_open_input(&FormatCtx, argv[1], NULL, NULL) != 0)
    {
        return -1;
    }
    if (avformat_find_stream_info(FormatCtx, NULL) < 0 )
        return -1;
    av_dump_format(FormatCtx, 0, argv[1], 0);

    int video_stream = -1;
    AVCodecContext * codecCtxOrig = nullptr;
    AVCodecContext * codecCtx = nullptr;

    for (int i = 0; i < FormatCtx->nb_streams; i++) {
        if (FormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO){
            video_stream = i;
            break;
        }
    }

    if (video_stream == -1)
        return -1;

    codecCtx = FormatCtx->streams[video_stream]->codec;

    AVCodec * pCodec = nullptr;
    pCodec = avcodec_find_decoder(codecCtx->codec_id);
    if (pCodec == NULL){
        cerr << "UNSUPPORTED CODEC" << endl;
        return -1;
    }

    codecCtx = avcodec_alloc_context3(pCodec);
    if (avcodec_copy_context(codecCtx, codecCtxOrig) != 0)
    {
        cerr << "Couldn't copy context" << endl;
        return -1;
    }

    if (avcodec_open2(codecCtx, pCodec, NULL) < 0)
    {
        return -1;
    }

    AVFrame * pFrame = nullptr;
    pFrame = av_frame_alloc();
    if (pFrame == NULL)
        return -1;

    AVFrame * pFrameRGB = nullptr;
    pFrameRGB = av_frame_alloc();
    if (pFrameRGB == NULL)
        return -1;

    uint8_t * buffer = nullptr;
    int numBytes;
    numBytes = avpicture_get_size(AVPixelFormat::AV_PIX_FMT_RGB24, codecCtx->width, codecCtx->height);
    buffer = (uint8_t *)av_malloc(numBytes * sizeof (uint8_t));
    avpicture_fill((AVPicture *)pFrameRGB, buffer, AV_PIX_FMT_RGB24, codecCtx->width, codecCtx->height);

    struct SwsContext * sws_ctx = nullptr;
    int frameFinished;
    AVPacket packet;

    sws_ctx = sws_getContext(codecCtx->width, codecCtx->height, codecCtx->pix_fmt, codecCtx->width, codecCtx->height, AV_PIX_FMT_RGB24, SWS_BILINEAR, NULL, NULL, NULL);
    int i = 0;
    while (av_read_frame(FormatCtx, &packet) >= 0)
    {
        if (packet.stream_index == video_stream)
        {
            avcodec_decode_video2(codecCtx, pFrame, &frameFinished, &packet);
            if (frameFinished)
            {
                sws_scale(sws_ctx, (uint8_t const * const *)pFrame->data, pFrame->linesize, 0, codecCtx->height, pFrameRGB->data, pFrameRGB->linesize);
                if (++i <= 5)
                    SaveFrame(pFrameRGB, codecCtx->width, codecCtx->height, i);
            }
        }
        av_free_packet(&packet);
    }

    // Free the RGB image
    av_free(buffer);
    av_free(pFrameRGB);

    // Free the YUV frame
    av_free(pFrame);

    // Close the codecs
    avcodec_close(codecCtx);
    avcodec_close(codecCtxOrig);

    // Close the video file
    avformat_close_input(&FormatCtx);
    return 0;
}
