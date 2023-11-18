#include "aacencoder.h"

AACEncoder::AACEncoder()
{

}

AACEncoder::~AACEncoder()
{
    if(_ctx)
    {
        avcodec_free_context(&_ctx);
        _ctx = NULL;
    }
//    if(_codec)
//    {
////        av_codec = NULL;
//    }
}

RET_CODE AACEncoder::Init(const Properties &properties)
{
    //获取参数
    _sample_rate = properties.GetProperty("sample_rate",48000);
    _bitrate = properties.GetProperty("bitrate",128*1024);
    _channels = properties.GetProperty("channels",2);
    _channels_layout = properties.GetProperty("channels_layout",
                                              (int)av_get_default_channel_layout(_channels));
    _codec = avcodec_find_encoder(AV_CODEC_ID_AAC);
    if(!_codec)
    {
        LogError("AAC: No encoder found");
        return RET_ERR_MISMATCH_CODE;
    }
    //分配编码器上下文
    _ctx = avcodec_alloc_context3(_codec);
    if(!_codec)
    {
        LogError("ctx: No econtext3 found");
        return RET_ERR_OUTFMEMORY;
    }
    _ctx->channels = _channels;
    _ctx->channel_layout = _channels_layout;
    _ctx->sample_fmt = AV_SAMPLE_FMT_FLTP;   //如果是fdk-aac不一样
    _ctx->sample_rate = _sample_rate;
    _ctx->bit_rate = _bitrate;

    if(avcodec_open2(_ctx,_codec,NULL)<0)
    {
        LogError("AAC:cant avcodec_open2");
        avcodec_free_context(&_ctx);
        return RET_FAIL;
    }


    return RET_OK;



}
RET_CODE AACEncoder::GetAdtsHeader(uint8_t *adts_header, int aac_length)
{
    uint8_t freqIdx = 0;    //0: 96000 Hz  3: 48000 Hz 4: 44100 Hz
    switch (_ctx->sample_rate)
    {
    case 96000: freqIdx = 0; break;
    case 88200: freqIdx = 1; break;
    case 64000: freqIdx = 2; break;
    case 48000: freqIdx = 3; break;
    case 44100: freqIdx = 4; break;
    case 32000: freqIdx = 5; break;
    case 24000: freqIdx = 6; break;
    case 22050: freqIdx = 7; break;
    case 16000: freqIdx = 8; break;
    case 12000: freqIdx = 9; break;
    case 11025: freqIdx = 10; break;
    case 8000: freqIdx = 11; break;
    case 7350: freqIdx = 12; break;
    default:
        LogError("can't support sample_rate:%d");
        freqIdx = 4;
        return RET_FAIL;
    }
    uint8_t ch_cfg = _ctx->channels;
    uint32_t frame_length = aac_length + 7;
    adts_header[0] = 0xFF;
    adts_header[1] = 0xF1;
    adts_header[2] = ((_ctx->profile) << 6) + (freqIdx << 2) + (ch_cfg >> 2);
    adts_header[3] = (((ch_cfg & 3) << 6) + (frame_length  >> 11));
    adts_header[4] = ((frame_length & 0x7FF) >> 3);
    adts_header[5] = (((frame_length & 7) << 5) + 0x1F);
    adts_header[6] = 0xFC;
    return RET_OK;
}

AVCodecContext *AACEncoder::getctx()
{
    return _ctx;
}
RET_CODE AACEncoder::Encode(AVFrame *frame, int64_t pts,int flush)
{
    int ret1 = 0;
    RET_CODE cod = RET_OK;
    if(!_ctx)
    {
        cod = RET_FAIL;
        LogError("AAC:no context");
    }
    if(frame)
    {
        frame->pts = pts;
        ret1 = avcodec_send_frame(_ctx,frame);
        if(ret1<0){
            if(ret1 == AVERROR(EAGAIN)) //赶紧读取packet，send不进去了
            {
                cod = RET_ERR_EAGAIN;
                LogWarn("send frame:please read packet quickly");
            }else if(ret1 == AVERROR_EOF)
            {
                cod = RET_ERR_EOF;
                LogWarn("send frame:RET_ERR_EOF");
            }else
            {
                cod = RET_FAIL;//真正报错，encoder就只能销毁了
                LogWarn("send frame:RET_FAIL");
            }
        }else
        {
            cod = RET_OK;
        }
    }
    if(flush)
    {//只能调用一次 这个函数的主要目的是重置解码器的内部状态，清空内部缓冲区，以便在特定情况下使用，例如在切换流或搜索时。
        avcodec_flush_buffers(_ctx);
    }
    return cod;

}
