#include "ffmpegengine.h"

#include <QApplication>
#include <QFileDialog>
#include <QMessageBox>
#include <QtDebug>
#include <QFileInfo>

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
}


void SaveFrame(AVFrame *pFrame, int widht, int height, int iFrame)
{
    FILE *pFile;
    char  szFilename[32];
    int y;

    sprintf(szFilename, "frame%d.ppm", iFrame);
    pFile = fopen(szFilename, "wb");
    if (pFile == nullptr)
        return;

    fprintf(pFile, "P6\n%d %d\n255\n", widht, height);

    for (y = 0; y < height; y++)
        fwrite(pFrame->data[0] + y*pFrame->linesize[0], 1, widht*3, pFile);

    fclose(pFile);
}

FFmpegEngine::FFmpegEngine(QStringList files) :
    Files(files),
    FormatCtx(nullptr),
    CodecCtxOrig(nullptr),
    CodecCtx(nullptr),
    Codec(nullptr),
    Frame(nullptr),
    FrameRGB(nullptr),
    Buffer(nullptr),
    sws_ctx(nullptr),
    videoStream(-1),
    frameFinished(-1)
{
    av_register_all();
}

FFmpegEngine::~FFmpegEngine()
{
    av_free_packet(&Packet);
    av_free(Buffer);
    av_frame_free(&FrameRGB);
    av_frame_free(&Frame);
    avcodec_close(CodecCtx);
    avcodec_close(CodecCtxOrig);
    avformat_close_input(&FormatCtx);
}

void FFmpegEngine::run()
{
    QString errorStr;
    if (!Files.isEmpty())
    {
        int errorCode = avformat_open_input(&FormatCtx, qPrintable(Files.at(0)), nullptr, nullptr);
        if (errorCode != 0)
        {
            char err[1024];
            av_strerror(errorCode, err, 1024);
            errorStr = tr("Could not open file - %1").arg(err);
        } else {
            errorCode = avformat_find_stream_info(FormatCtx, nullptr);
            if (errorCode < 0)
            {
                char err[1024];
                av_strerror(errorCode, err, 1024);
                errorStr = tr("Could not find stream information - %1").arg(err);
            } else {
                av_dump_format(FormatCtx, 0, qPrintable(Files.at(0)), 0);
                parse_media();
            }
        }
        avformat_close_input(&FormatCtx);
    }
}

void FFmpegEngine::parse_media()
{
    for (int i = 0; i < int(FormatCtx->nb_streams); i++) {
        if (avcodec_find_decoder(FormatCtx->streams[i]->codecpar->codec_id) == nullptr)
            qCritical() << "Unsupported codec in stream" << i << "of file" << Files.first().data();

        if (FormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO && FormatCtx->streams[i]->codecpar->width > 0
                && FormatCtx->streams[i]->codecpar->height > 0) {
            videoStream = i;
            break;
        }
    }

        CodecCtxOrig = FormatCtx->streams[videoStream]->codec;
        Codec = avcodec_find_decoder(CodecCtxOrig->codec_id);
        if (Codec == nullptr){
            qCritical() << "Unsupported codec" << videoStream << "of file" << Files.first().data();
        }

        CodecCtx = avcodec_alloc_context3(Codec);
        if (avcodec_copy_context(CodecCtx, CodecCtxOrig) != 0) {
            qCritical() << "Couldn't copy codec context" << videoStream << "of context" << "CodecCtxOrig";
        }

        if (avcodec_open2(CodecCtx, Codec, nullptr) < 0)
            qWarning() << "Couldn't open codec";

        Frame = av_frame_alloc();
        FrameRGB = av_frame_alloc();
        if (FrameRGB == nullptr)
            qWarning() << "Can not allocate for FrameRGB";

        int numBytes = avpicture_get_size(AV_PIX_FMT_RGB24, CodecCtx->width, CodecCtx->height);
        Buffer = (uint8_t *)av_malloc(numBytes * sizeof (uint8_t));
        avpicture_fill((AVPicture *)FrameRGB, Buffer, AV_PIX_FMT_RGB24, CodecCtx->width, CodecCtx->height);
        sws_ctx = sws_getContext(CodecCtx->width, CodecCtx->height, CodecCtx->pix_fmt, CodecCtx->width, CodecCtx->height, AV_PIX_FMT_RGB24, SWS_BILINEAR, nullptr, nullptr, nullptr);

        int j = 0;
        while (av_read_frame(FormatCtx, &Packet) >= 0)
        {
            if (Packet.stream_index == videoStream) {
                avcodec_decode_video2(CodecCtx, Frame, &frameFinished, &Packet);

                if (frameFinished) {
                    sws_scale(sws_ctx, (uint8_t const * const *)Frame->data, Frame->linesize, 0, CodecCtx->height, FrameRGB->data, FrameRGB->linesize);

                    if (++j <= 5)
                        SaveFrame(FrameRGB, CodecCtx->width, CodecCtx->height, j);
                }
            }

        }
}

void FFmpegEngine::process_file_list(QStringList& files)
{
    if (QFileInfo(files.at(0)).isDir()){
        QString folder_name = files.at(0).mid(files.at(0).lastIndexOf('/') + 1);
        QDir directory(files.at(0));
        directory.setFilter(QDir::NoDotAndDotDot | QDir::AllEntries);
        QFileInfoList subdir_files = directory.entryInfoList();
        QStringList subdir_filenames;

        for (int j = 0; j < subdir_files.size(); j++)
            subdir_filenames.append(subdir_files.at(j).filePath());
    }
}
