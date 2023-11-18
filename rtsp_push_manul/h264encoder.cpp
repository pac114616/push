#include "h264encoder.h"

H264Encoder::H264Encoder()
{

}

H264Encoder::~H264Encoder()
{
    if(_ctx)
        avcodec_free_context(&_ctx);//内部会关闭编码器
    if(_frame)
        av_frame_free(&_frame);
}

int H264Encoder::Init(const Properties &properties)
{
    int ret = 0;
    _width = properties.GetProperty("width",0);
    if(_width == 0 || _width%2 != 0)
    {
        LogError("error width:%d",_width);
        return RET_ERR_NOT_SUPPORT;
    }
    _height = properties.GetProperty("height",0);
    if(_height == 0 || _height%2 != 0)
    {
        LogError("height:%d",_height);
        return RET_ERR_NOT_SUPPORT;
    }
    _fps = properties.GetProperty("fps",25);
    b_frames = properties.GetProperty("b_frames",0);
    _biterate = properties.GetProperty("bitrate",500*1024);
    _gop = properties.GetProperty("gop",_fps);
    _pix_fmt = properties.GetProperty("pix_fmt",AV_PIX_FMT_YUV420P);
    _codec_name = properties.GetProperty("codec_name","default");
    if(_codec_name == "default")
    {
        LogInfo("use default encoder");
        _codec = avcodec_find_encoder(AV_CODEC_ID_H264);
    }else
    {
        LogInfo("use %s encoder",_codec_name.c_str());
        _codec = avcodec_find_encoder_by_name(_codec_name.c_str());
    }
    //查找H264编码器，确定是否存在
    if(!_codec)
    {
        LogInfo("find video encoder fail");
        return RET_FAIL;
    }
    _ctx = avcodec_alloc_context3(_codec);
    if(!_ctx)
    {
        LogInfo("avcodec_alloc_context3 h264 fail");
        return RET_FAIL;
    }
    _ctx->width = _width;
    _ctx->height = _height;

    //码率
    _ctx->bit_rate = _biterate;
    //GOP 以一帧数据为GOP 太大会影响秒开
    _ctx->gop_size = _gop;
    //帧率
    _ctx->framerate.num = _fps;
    _ctx->framerate.den = 1;
    //time_base
    _ctx->time_base.num = 1;
    _ctx->time_base.den = _fps;
    //像素格式
    _ctx->pix_fmt = (AVPixelFormat)_pix_fmt;

    _ctx->codec_type = AVMEDIA_TYPE_VIDEO;

    _ctx->max_b_frames = b_frames;

    av_dict_set(&_dict,"preset","medium",0);
    av_dict_set(&_dict,"tune","zerolatency",0);
    av_dict_set(&_dict,"profile","high",0);
    _ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    //初始化编码器
    ret = avcodec_open2(_ctx,_codec,&_dict);
    if(ret <0 )
    {
        char buf[1024] = {0};
        av_strerror(ret,buf,sizeof(buf) - 1);
        LogError("avcodec_open2 failed:%s",buf);
        return RET_FAIL;
    }

    //从extradata读取sps pps
    if(_ctx->extradata)
    {
        LogInfo("extradata_size:%d",_ctx->extradata_size);
        //第一个为sps 7
        //第二个为pps 8
        uint8_t *sps = _ctx->extradata + 4;//直接跳到数据
        int sps_len = 0;
        uint8_t *pps = NULL;
        int pps_len = 0;
        uint8_t *data = _ctx->extradata+4;
        for(int i=0;i<_ctx->extradata_size - 4;++i)
        {
            if(0==data[i]&&0==data[i+1]&& 0 == data[i+2] && 1 == data[i+3])
            {
                pps = &data[i+4];//找到pps起始位置
                break;
            }
        }
        sps_len = int(pps - sps) -4 ;//4是00 00 00 01占用的字节
        pps_len = _ctx->extradata_size - 4*2 -sps_len;
        _sps.append(sps,sps+sps_len);
        _pps.append(pps,pps+pps_len);

    }

    _frame = av_frame_alloc();
    _frame->width = _width;
    _frame->height = _height;
    _frame->format = _ctx->pix_fmt;
    ret = av_frame_get_buffer(_frame,0);

}

AVCodecContext *H264Encoder::getCtx()
{
    return _ctx;
}
//virtual AVPacket *Encode(uint8_t *yuv,int size,const int64_t pts,int *pkt_frame,RET_CODE *ret);
RET_CODE H264Encoder::Encode(uint8_t *yuv,int size,int64_t pts)
{
    int ret1 = 0;
    RET_CODE ret_code = RET_OK;
    int need_size = 0;
    if(yuv)
    {
        need_size = av_image_fill_arrays(_frame->data,_frame->linesize,yuv,
                                             (AVPixelFormat)_frame->format,_frame->width,
                                             _frame->height,1);

        if(need_size != size)
        {
           return RET_FAIL;
        }
        _frame->pts = pts;
        _frame->pict_type = AV_PICTURE_TYPE_NONE;
        ret1 = avcodec_send_frame(_ctx,_frame);
    }else
    {
        ret1 = avcodec_send_frame(_ctx,NULL); //只处理一次，再传会忽略
    }
    if(ret1<0)
    {
        char buf[1024] = {0};
        av_strerror(ret1,buf,sizeof(buf)-1);
        LogError("avcodec_send_frame failed:%s",buf);
        if(ret1==AVERROR(EAGAIN))
        {
            ret_code = RET_ERR_EAGAIN;
        }else if(ret1 == AVERROR_EOF)
        {
            ret_code = RET_ERR_EOF;
        }else
        {
            ret_code = RET_FAIL; //真正报错，需要销毁encoder
        }
    }
    return ret_code;

}
