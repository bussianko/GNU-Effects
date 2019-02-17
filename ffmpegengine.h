#ifndef FFMPEGENGINE_H
#define FFMPEGENGINE_H

#include <QObject>
#include <QFile>

extern "C"{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
}

class FFmpegEngine : public QObject
{
private:
    QStringList         Files;
    AVFormatContext *   FormatCtx;
    AVCodecContext  *   CodecCtxOrig;
    AVCodecContext  *   CodecCtx;
    AVCodec         *   Codec;
    AVFrame         *   Frame;
    AVFrame         *   FrameRGB;
    AVPacket            Packet;
    int                 videoStream;
    int                 frameFinished;
    uint8_t         *   Buffer;
    struct SwsContext * sws_ctx;
    void parse_media();
    void process_file_list(QStringList& files);
public:
    FFmpegEngine(QStringList  files);
    ~FFmpegEngine();
    void run();
};

#endif // FFMPEGENGINE_H
