#ifndef AACENCODER_H
#define AACENCODER_H
extern "C" {
#include <libavcodec/avcodec.h>
}

#include "mediabase.h"
class AACEncoder
{
public:
    AACEncoder();
    virtual ~AACEncoder();
    //smaple_rate sample_fmt channels bitrate
    RET_CODE Init(const Properties &properties);
    //*pkt_frame = 0,receive_packet报错，*pkt_frame = 1,send_frame报错 RET_OK不需要做异常处理
    RET_CODE Encode(AVFrame *frame, int64_t pts,int flush);
    RET_CODE GetAdtsHeader(uint8_t *adts_header, int aac_length);
    AVCodecContext *getctx();
    virtual int getFormat()
    {
        return _ctx->sample_fmt;
    }
    virtual int getFrameSamples()
    {
        return _ctx->frame_size; //采样点数 单个通道
    }
    virtual int getChannelLayout() {
        return _ctx->channel_layout;
    }
    virtual int getChannels()
    {
        return _ctx->channels; //通道数
    }
    virtual int getSampleRate()
    {
        return _ctx->sample_rate; //通道数
    }
    virtual int getFrameBytes()
    {
        return av_get_bytes_per_sample(_ctx->sample_fmt)*_ctx->channels*_ctx->frame_size ; //一帧数据占用的字节数
    }
    AVCodecContext *getCodecContext()
    {
        return _ctx;
    }   
private:
    int _sample_rate = 48000;
    int _channels = 2;
    int _bitrate = 128*1024; //48000*16*2
    int _channels_layout = AV_CH_LAYOUT_STEREO;
    AVCodec *_codec = NULL; //初始化
    AVCodecContext *_ctx = NULL;

};

#endif // AACENCODER_H
