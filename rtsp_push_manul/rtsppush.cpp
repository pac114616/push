#include "rtsppush.h"
#include "timesutil.h"
RtspPush::RtspPush(MessageQueue *msg_queue)
    :_msg_queue(msg_queue)
{
    LogInfo("RtspPush create!");
}

RtspPush::~RtspPush()
{
    DeInit();
}
//1 继续阻塞，0退出阻塞
static int decode_interrupt_cb(void *ctx)
{
    RtspPush *_rtsp_pusher = (RtspPush *)ctx;
    if(_rtsp_pusher->isTimeout())
    {
        LogWarn("timeout:%dms",_rtsp_pusher->getTimeout());
        return 1;
    }
    LogInfo("block time:%lld",_rtsp_pusher->getBlockTime());
    return 0;
}
RET_CODE RtspPush::Init(const Properties &properties)
{
    char str_err[512] = {0};
    _url = properties.GetProperty("url","");
    _audio_frame_duration = properties.GetProperty("audio_frame_duration",0);
    _video_frame_duration = properties.GetProperty("video_frame_duration",0);
    //默认udp
    _rtsp_transport = properties.GetProperty("rtsp_transport","");
    //默认5秒
    _timeout = properties.GetProperty("timeout",5000);
    _max_queue_duration = properties.GetProperty("max_queue_duration",500);
    if(_url == "")
    {
        LogInfo("url is null");
        return RET_FAIL;
    }
    if(_rtsp_transport == "")
    {
        LogInfo("rtsp_transport is null,use udp or tcp");
        return RET_FAIL;
    }
    int ret = 0;
    //初始化网络库
    ret = avformat_network_init();
    if(ret<0)
    {
        av_strerror(ret,str_err,sizeof(str_err)-1);
        LogInfo("avformat_network_init failed:%s",str_err);
        return RET_FAIL;
    }
    //分配上下文AVFormatContext
    ret = avformat_alloc_output_context2(&_fmt_ctx,NULL,"rtsp",_url.c_str());
    if(ret<0)
    {
        av_strerror(ret,str_err,sizeof(str_err)-1);
        LogInfo("avformat_alloc_output failed:%s",str_err);
        return RET_FAIL;
    }
    ret = av_opt_set(_fmt_ctx->priv_data,"rtsp_transport",_rtsp_transport.c_str(),0);
    if(ret<0)
    {
        av_strerror(ret,str_err,sizeof(str_err)-1);
        LogInfo("av_opt_set failed:%s",str_err);
        return RET_FAIL;
    }
    //回调函数原型 读取音频帧或者写入音频帧的时候调用
    _fmt_ctx->interrupt_callback.callback = decode_interrupt_cb;//设置超时回调
    _fmt_ctx->interrupt_callback.opaque = this; //代表的是当前rtsp对象
    //创建队列
    _queue = new PacketQueue(_audio_frame_duration,_video_frame_duration);
    if(!_queue)
    {
        LogError("new PacketQueue failed");
        return RET_ERR_OUTFMEMORY;
    }
    return RET_OK;
}

void RtspPush::DeInit()
{
    if(_queue)
    {
        _queue->Abort(); //把流停掉，后面push等不会阻塞

    }
    Stop();
    if(_fmt_ctx)
    {
        avformat_free_context(_fmt_ctx); //不用将avstream置空
        _fmt_ctx = NULL;
    }
    if(_queue)
    {
        delete _queue;
        _queue = NULL;
    }

}

RET_CODE RtspPush::pushData(AVPacket *pkt, Media_type media_type)
{
    int ret = _queue->Push(pkt,media_type);
    if(ret<0)
    {
        return RET_FAIL;
    }else
    {
        return RET_OK;
    }
}

RET_CODE RtspPush::pushConnect()
{
    if(!_video_stream && !_audio_stream)
    {
        LogInfo("video stream and audio stream is null");
        return RET_FAIL;
    }
    restTimeout();
    //连接服务器
    int ret = avformat_write_header(_fmt_ctx,NULL);
    if(ret<0)
    {
        char str_err[512] = {0};
        av_strerror(ret,str_err,sizeof(str_err)-1);
        LogInfo("avformat_network_init failed:%s",str_err);
        return RET_FAIL;
    }
    LogInfo("push connect success");
    return Start();
}

RET_CODE RtspPush::configVideoStream(const AVCodecContext *ctx)
{
    if(!_fmt_ctx)
    {
        LogError("fmt_ctx is null");
        return  RET_FAIL;
    }
    if(!ctx)
    {
        LogError("ctx is null");
        return RET_FAIL;
    }
    //添加视频流
    AVStream *vs = avformat_new_stream(_fmt_ctx,NULL);
    if(!vs)
    {
        LogError("avofrmat_new_stream failed");
        return RET_FAIL;
    }
    vs->codecpar->codec_tag = 0;
    //从编码器拷贝信息
    avcodec_parameters_from_context(vs->codecpar,ctx);
    _video_ctx = (AVCodecContext *)ctx;
    _video_stream = vs;
    _video_index = vs->index; //整个索引非常重要，根据_fmt_ctx判别音视频包
    return RET_OK;
}

RET_CODE RtspPush::configAudioStream(const AVCodecContext *ctx)
{
    if(!_fmt_ctx)
    {
        LogError("fmt_ctx is null");
        return  RET_FAIL;
    }
    if(!ctx)
    {
        LogError("ctx is null");
        return RET_FAIL;
    }
    //添加音频流
    AVStream *as = avformat_new_stream(_fmt_ctx,NULL);
    if(!as)
    {
        LogError("avofrmat_new_stream failed");
        return RET_FAIL;
    }
    as->codecpar->codec_tag = 0;
    //从编码器拷贝信息
    avcodec_parameters_from_context(as->codecpar,ctx);
    _audio_ctx = (AVCodecContext *)ctx;
    _audio_stream = as;
    _audio_index = as->index; //整个索引非常重要，根据_fmt_ctx判别音视频包
    return RET_OK;
}

void RtspPush::Loop()
{
    LogInfo("rtsp loop into");
    int ret = 0;
    AVPacket *pkt = NULL;
    Media_type mediaType;
    while(true)
    {
        if(_request_abort)
        {
            LogInfo("abort request");
            break;
        }
        debugQueue(_debug_interval);//间隔2秒打印一次
        checkPacketQueueDuration();  //可以每隔1秒检查一次 自己实现
        ret = _queue->PopWithTimeout(&pkt,mediaType,2000);
        if(ret == 1) //1代表读取到消息
        {
            if(_request_abort)
            {
                LogInfo("abort request");
                av_packet_free(&pkt);
                break;
            }
            switch (mediaType) {
            case E_VIDEO_TYPE:
                ret = sendPacket(pkt,mediaType);
                if(ret<0)
                {
                    LogInfo("send video packet failed");
                }
                av_packet_free(&pkt);
                break;
            case E_AUDIO_TYPE:
                ret = sendPacket(pkt,mediaType);
                if(ret<0)
                    LogInfo("send audio packet failed");
                av_packet_free(&pkt);
                break;
            default:
                LogError("unknown media type");
                break;
            }
        }
    }
    restTimeout();
    ret = av_write_trailer(_fmt_ctx);//结束
    if(ret < 0) {
        char str_error[512] = {0};
        av_strerror(ret, str_error, sizeof(str_error) -1);
        LogError("av_write_trailer failed:%s", str_error);
        return;
    }
    LogInfo("avformat_write_header ok");
}

bool RtspPush::isTimeout()
{
    if(TimesUtil::GetTimeMillisecond()-_pre_time >_timeout)
        return true;
    return false;
}

void RtspPush::restTimeout()
{
    _pre_time = TimesUtil::GetTimeMillisecond();  //重置为当前时间

}

int RtspPush::getTimeout()
{
    return _timeout;
}

int64_t RtspPush::getBlockTime()
{
    return TimesUtil::GetTimeMillisecond()-_pre_time;
}

void RtspPush::debugQueue(int64_t interval)
{
    int64_t cur_time = TimesUtil::GetTimeMillisecond();
    if(cur_time - _pre_debug_time>interval)
    {
        //打印信息
        packet_queue_stats stats;
        _queue->getStats(&stats);
        LogInfo("duration:a-%lldms,v-%lldms",stats.audio_duration,stats.video_duration);
        _pre_debug_time = cur_time;
    }
}

void RtspPush::checkPacketQueueDuration()
{
    packet_queue_stats stats;
    _queue->getStats(&stats);
    if(stats.audio_duration>_max_queue_duration||stats.video_duration>_max_queue_duration)
    {
        _msg_queue->notify_msg3(MSG_RTSP_QUEUE_DURATION,stats.audio_duration,stats.video_duration);
        LogWarn("drop packet -> a:%lld,v:%lld,th:%d",stats.audio_duration,stats.video_duration,
                _max_queue_duration);
        _queue->Drop(false,_max_queue_duration);
    }
}

int RtspPush::sendPacket(AVPacket *pkt, Media_type mediaType)
{
    AVRational dst_timebase;
    AVRational src_timebase={1,1000}; //采集编码时间戳单位都是ms
    if(E_VIDEO_TYPE == mediaType)
    {
        pkt->stream_index = _video_index;
        dst_timebase = _video_stream->time_base;
    }else if(E_AUDIO_TYPE == mediaType)
    {
        pkt->stream_index = _audio_index;
        dst_timebase = _audio_stream->time_base;
    }else
    {
        LogError("unknown mediatype:%d", mediaType);
        return -1;
    }
    pkt->pts = av_rescale_q(pkt->pts,src_timebase,dst_timebase);
    pkt->duration = 0;
    //如果传输中断(比如服务器突然断开),也会报错
    restTimeout();
    int ret = av_write_frame(_fmt_ctx,pkt);

    if(ret<0)
    {
        _msg_queue->notify_msg2(MSG_RTSP_ERROR,ret);
        char str_err[512] = {0};
        av_strerror(ret,str_err,sizeof(str_err)-1);
        LogInfo("avformat_network_init failed:%s",str_err);
        return -1;
    }
    return 0;

}
