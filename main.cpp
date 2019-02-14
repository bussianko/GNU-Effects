extern "C"{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
}

#include <iostream>

void SaveFrame(AVFrame * pFrame, int width, int height, int iFrame)
{
    FILE *pFile;
    char szFileName[32];
    int y;
    
    // Открываем файл
    sprintf(szFileName, "frame%d.ppm", iFrame);
    pFile = fopen(szFileName, "wb");
    if (pFile == NULL)
        return;
    
    //Записываем заголовок
    fprintf(pFile, "P6\n%d %d\n255\n", width, height);
    
    //Записываем данные пикселя
    for (y = 0; y < height; y++)
        fwrite(pFrame->data[0] + y * pFrame->linesize[0], 1 , width*3, pFile);
    
    //Закрываем файл
    fclose(pFile);
}

using namespace std;

int main(int argc, char **argv) {
    
    AVFormatContext *   pFormatCtx = nullptr;
    int                 i, videoStream;
    AVCodecContext  *   pCodecCtxOrig = nullptr;
    AVCodecContext  *   pCodecCtx = nullptr;
    AVCodec         *   pCodec = nullptr;
    AVFrame         *   pFrame = nullptr;
    AVFrame         *   pFrameRGB = nullptr;
    AVPacket            packet;
    int                 frameFinished;
    int                 numBytes;
    uint8_t         *   buffer = nullptr;
    struct SwsContext * sws_ctx = nullptr;
    
    if (argc < 2)
    {
        cout << "Please provide a movie file" << endl;
        return -1;
    }
    
    //Регестрируем все форматы и кодеки
    av_register_all();
    
    //Открываем видео файл
    if (avformat_open_input(&pFormatCtx, argv[1], NULL, NULL) != 0)
        return -1;
    
    //Извлекаем инвормацию потока
    if (avformat_find_stream_info(pFormatCtx, NULL) < 0)
        return -1;
    
    //Печатаем информацию о файле в стандартный поток ошибок
    av_dump_format(pFormatCtx, 0, argv[1], 0);
    
    //Ищем первый поток видео
    videoStream = -1;
    for (i = 0; i < pFormatCtx->nb_streams; i++)
        if (pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            videoStream = i;
            break;
        }
        
    if (videoStream == -1)
        return -1;
    
    //Получаем на контекст кодека для видео потока
    pCodecCtxOrig = pFormatCtx->streams[videoStream]->codec;
    //Ищем декодер для видео потока
    pCodec = avcodec_find_decoder(pCodecCtxOrig->codec_id);
    if (pCodec == NULL)
    {
        cerr << "Unsupported codec!" << endl;
        return -1;
    }
    
    //Копируем контекст
    pCodecCtx = avcodec_alloc_context3(pCodec);
    if (avcodec_copy_context(pCodecCtx, pCodecCtxOrig) != 0)
    {
        cerr << "Couldn't copy codec context";
        return -1;
    }
    
    //Открываем кодек
    if (avcodec_open2(pCodecCtx, pCodec, NULL) < 0)
        return -1; //Не смог открыть кодек
        
    //Выделение памяти под video frame
    pFrame = av_frame_alloc();
    //Выделение памяти для AVFrame structure
    pFrameRGB = av_frame_alloc();
    if (pFrameRGB == NULL)
        return -1; //Память не выделенна
        
    //Определяем необходимый размер буфера и выделяем память под буфера
    numBytes = avpicture_get_size(AV_PIX_FMT_RGB24, pCodecCtx->width,      pCodecCtx->height);
    buffer = (uint8_t *)av_malloc(numBytes * sizeof(uint8_t));
    avpicture_fill((AVPicture *)pFrameRGB, buffer, AV_PIX_FMT_RGB24, pCodecCtx->width, pCodecCtx->height);
    //Инициализируем SWS Cpontext для програмног скалирования
    sws_ctx = sws_getContext(pCodecCtx->width, pCodecCtx->height, pCodecCtx->pix_fmt, pCodecCtx->width, pCodecCtx->height, AV_PIX_FMT_RGB24, SWS_BILINEAR, NULL, NULL, NULL);
    
    //Читаем фреймы и сохраняем первые 5 фреймов на диск
    i = 0;
    while (av_read_frame(pFormatCtx, &packet) >= 0)
    {
        //Этот пакет из видео потока?
        if (packet.stream_index == videoStream)
        {
            //Декодируем видео фреймы
            avcodec_decode_video2(pCodecCtx, pFrame, &frameFinished, &packet);
            
            //Получили видео фрейм?
            if (frameFinished)
            {
                //Конвертируем картинку из нативного формата в формат RGB
                sws_scale(sws_ctx, (uint8_t const * const *)pFrame->data, pFrame->linesize, 0, pCodecCtx->height, pFrameRGB->data, pFrameRGB->linesize);
                
                //Сохраняем полученные фреймы на диск
                if (++i <= 5)
                    SaveFrame(pFrameRGB, pCodecCtx->width, pCodecCtx->height, i);
            }
        }
        //Освобождаем packet которому распределили память из кучи функцией av_read_frame
        av_free_packet(&packet);
    }
    //Освобождаем память выделенную под RGB image
    av_free(buffer);
    av_frame_free(&pFrameRGB);
    
    //Освобождаем YUV frame
    av_frame_free(&pFrame);
    
    //Закрываем кодеки
    avcodec_close(pCodecCtx);
    avcodec_close(pCodecCtxOrig);
    
    //Закрываем видео файл
    avformat_close_input(&pFormatCtx);
    
    return 0;
}
