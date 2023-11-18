#include "pushwork.h"
#include "dlog.h"
#include <functional>
PushWork::PushWork(MessageQueue *msg_queue):_msg_queue(msg_queue)
{
    //
}

PushWork::~PushWork()
{
    //从源头开始释放数据 如果先停止编码器，但是音视频还在回调，会出现异常
    DeInit();
    LogInfo("析构完成");
}

RET_CODE PushWork::Init(const Properties &properties)
{
//    int ret = 0;
    // 音频test模式
    _audio_test = properties.GetProperty("audio_test", 1);
    _input_pcm_name = properties.GetProperty("input_pcm_name", "48000_2_s16le.pcm");

    // 麦克风采样属性
    mic_sample_rate_ = properties.GetProperty("mic_sample_rate", 48000);
    mic_sample_fmt_ = properties.GetProperty("mic_sample_fmt", AV_SAMPLE_FMT_S16);
    mic_channels_ = properties.GetProperty("mic_channels", 2);

    //视频test模式
    video_test = properties.GetProperty("desktop_x",0);
    _input_yuv_name = properties.GetProperty("input_yuv_name","input_1280_720_420p.yuv");

    //桌面录制属性
    _desktop_x = properties.GetProperty("desktop_x",0);
    _desktop_y = properties.GetProperty("desktop_y",0);
    _desktop_width = properties.GetProperty("desktop_width",1920);
    _desktop_height = properties.GetProperty("desktop_height",1080);
    _desktop_fps = properties.GetProperty("desktop_fps",25);

    //视频编码属性
    _video_width = properties.GetProperty("video_width",_desktop_width);
    _video_height = properties.GetProperty("video_height",_desktop_height);
    _video_fps = properties.GetProperty("video_fps",_desktop_fps);
    _video_gop = properties.GetProperty("video_gop",_video_fps);
    _video_bitrate = properties.GetProperty("video_bitrate", 1024*1024);   // 先默认1M fixedme
    _video_b_frames = properties.GetProperty("video_b_frames", 0);   // b帧数量
    //rtsp 参数
    _rtsp_url = properties.GetProperty("rtsp_url","");
    _rtsp_transport = properties.GetProperty("rtsp_transport","");
    _rtsp_timeout = properties.GetProperty("timeout",5000);
    _rtsp_max_queue_duration = properties.GetProperty("rtsp_max_queue_duration",100);
    //初始化publish time start_time_
    AVPublishTime::GetInstance()->Rest(); //推流打时间戳的问题
    //设置音频编码器，先音频捕获初始化
    _audio_encoder = new AACEncoder();
    if(!_audio_encoder)
    {
        LogError("new AACEncoder() failed");
        return RET_FAIL;
    }
    Properties aud_codec_properties;
    aud_codec_properties.SetProperty("sample_rate",_audio_sample_rate);
    aud_codec_properties.SetProperty("channels",_audio_channels);
    aud_codec_properties.SetProperty("bitrate",_audio_bitrate);
    //需要什么样的采样格式是从编码器读取出来的
    if(_audio_encoder->Init(aud_codec_properties)!=RET_OK)
    {
        LogError("AACEncoder Init failed");
        return RET_FAIL;
    }
    //重采样
    _audio_ctx = _audio_encoder->getctx();
    _audio_resample = new AudioResample();
    if(!_audio_resample)
    {
        LogError("new AudioResample() failed");
        return RET_FAIL;
    }
    if(_audio_resample->InitS16ToFltp(mic_channels_,mic_sample_rate_,_audio_channels,_audio_sample_rate)!=RET_OK)
    {
        LogError("new InitS16ToFltp failed");
        return RET_FAIL;
    }

    int frame_bytes2 = 0;
    // 默认读取出来的数据是s16的，编码器需要的是fltp, 需要做重采样
    // 手动把s16转成fltp 8192
    _fltp_buf_size = av_samples_get_buffer_size(NULL, _audio_encoder->getChannels(),
                                              _audio_encoder->getFrameSamples(),
                                              (enum AVSampleFormat)_audio_encoder->getFormat(), 1);
    _fltp_buf = (uint8_t *)av_malloc(_fltp_buf_size);
    if(!_fltp_buf)
    {
        LogError("_fltp_buf_av_malloc Fail");
        return RET_ERR_OUTFMEMORY;
    }
    if(_audio_resample->allocFLTPFrame(_audio_frame,_audio_encoder->getFrameSamples(),
                                       _audio_encoder->getChannels(),_audio_encoder->getChannelLayout())!=RET_OK)
    {
        LogError("alloc fltp frame fail");
        return RET_FAIL;
    }
    frame_bytes2 = _audio_encoder->getFrameBytes();
    if(frame_bytes2!=_fltp_buf_size)
    {
        LogError("frame_bytes2:%d != _fltp_buf_size%d",frame_bytes2,_fltp_buf_size);
        return RET_FAIL;
    }


    _video_encoder = new H264Encoder();
    Properties vid_codec_propeties;
    vid_codec_propeties.SetProperty("width",_video_width);
    vid_codec_propeties.SetProperty("height",_video_height);
    vid_codec_propeties.SetProperty("fps",_video_fps);
    vid_codec_propeties.SetProperty("b_frames",_video_b_frames);
    vid_codec_propeties.SetProperty("video_bitrate",_video_bitrate);
    vid_codec_propeties.SetProperty("gop",_video_gop);
    if(_video_encoder->Init(vid_codec_propeties)!= RET_OK)
    {
        LogError("h264Encoder init fail");
        return RET_FAIL;
    }
    _video_ctx = _video_encoder->getCtx();
    //在音视频编码器初始化完，音视频捕获前
    _rtsp_push = new RtspPush(_msg_queue);
    if(!_rtsp_push)
    {
        LogError("new RTSP Push failed");
        return RET_FAIL;
    }
    Properties rtsp_properties;
    rtsp_properties.SetProperty("url",_rtsp_url);
    rtsp_properties.SetProperty("rtsp_transport",_rtsp_transport);
    rtsp_properties.SetProperty("timeout",_rtsp_timeout);
    rtsp_properties.SetProperty("max_queue_duration",_rtsp_max_queue_duration);
    if(_audio_encoder)
    {
        rtsp_properties.SetProperty("audio_frame_duration",_audio_encoder->getFrameSamples()*1000/_audio_encoder->getSampleRate());
    }
    if(_video_encoder)
        rtsp_properties.SetProperty("video_frame_duration",1000/_video_encoder->get_fps_size());

    if(_rtsp_push->Init(rtsp_properties) != RET_OK)
    {
        LogError("RTSP Push init failed");
        return RET_FAIL;
    }
    //创建音视频流
    if(_video_encoder)
    {
        if(_rtsp_push->configVideoStream(_video_encoder->getCodecContext())!=RET_OK)
        {
            LogError("RTSP Push configVideoStream failed");
            return RET_FAIL;
        }
    }
    if(_audio_encoder)
    {
        if(_rtsp_push->configAudioStream(_audio_encoder->getCodecContext())!=RET_OK)
        {
            LogError("RTSP Push configAudioStream failed");
            return RET_FAIL;
        }
    }
    //connect 成功会创建线程 然后loop
    if(_rtsp_push->pushConnect()!=RET_OK)
    {
        LogError("RTSP Push connect failed");
        return RET_FAIL;
    }
    _audio_capturer = new AudioCapture();
    if(!_audio_capturer)
    {
        LogError("new AAC Encoder() fail");
        return RET_FAIL;
    }
    //设置音频捕获
    Properties aud_cap_properties;
    aud_cap_properties.SetProperty("audio_test",1);
    aud_cap_properties.SetProperty("input_pcm_name",_input_pcm_name);
//    aud_cap_properties.SetProperty("nb_channels",mic_channels_);
    aud_cap_properties.SetProperty("channels",mic_channels_);
    aud_cap_properties.SetProperty("format",mic_sample_fmt_);
    aud_cap_properties.SetProperty("byte_per_sample",2);
    if(_audio_capturer->Init(aud_cap_properties)!=RET_OK)
    {
        LogError("AudioCapturer Init failed");
        return RET_FAIL;
    }
    _audio_capturer->AddCallback(std::bind(&PushWork::PcmCallback,this,std::placeholders::_1,
                                 std::placeholders::_2));
    if(_audio_capturer->Start()!=RET_OK)
    {
        LogError("AudioCaptured Start failed");
        return RET_FAIL;
    }
    _video_capturer = new VIdeoCapturer();
    //设置视频捕获
    Properties vid_cap_properties;
    vid_cap_properties.SetProperty("video_test",1);
    vid_cap_properties.SetProperty("input_yuv_name",_input_yuv_name);
    vid_cap_properties.SetProperty("width",_desktop_width);
    vid_cap_properties.SetProperty("height",_desktop_height);
    if(_video_capturer->Init(vid_cap_properties)!=RET_OK)
    {
        LogError("videoCapturer Init failed");
        return RET_FAIL;
    }
    _video_capturer->AddCallback(std::bind(&PushWork::YUVCallback,this,
                                           std::placeholders::_1,
                                           std::placeholders::_2));
    if(_video_capturer->Start()!=RET_OK)
    {
        LogError("AudioCaptured Start failed");
        return RET_FAIL;
    }
    return RET_OK;
}
void s16le_to_fltp(short *s16le,float *fltp,int nb_samples)
{
    float *fltp_l = fltp;
    float *fltp_r = fltp+nb_samples;
    for(int i=0;i<nb_samples;i++)
    {
        fltp_l[i] = s16le[i*2]/32768.0;  //0 2 4
        fltp_r[i] = s16le[i*2+1]/32768.0;//1 3 5
    }
}

void PushWork::getAndPutAudioAVPacket()
{

    while(1)
    {
        AVPacket *packet = av_packet_alloc();
        int ret1 = avcodec_receive_packet(_audio_ctx,packet);
        if(ret1<0)
        {
            av_packet_free(&packet);
            if(ret1 == AVERROR(EAGAIN)||ret1 == AVERROR_EOF)   //需要继续发送frame我们才有packet读取
            {
                LogWarn("receive audio packet is null");
                break;
            }else
            {
                LogError("receive audio packet is null");          //真正报错，这个encoder就只能销毁了
                break;
            }
        }else
        {
            if(!_aac_fp)
            {
                _aac_fp = fopen("push_dump.aac","wb");
                if(!_aac_fp)
                {
                    LogError("fopen push_dump.aac fail");
                    return;
                }
            }
            if(_aac_fp)
            {
                uint8_t adts_header[7];
                _audio_encoder->GetAdtsHeader(adts_header,packet->size);
                fwrite(adts_header,1,7,_aac_fp);
                fwrite(packet->data,1,packet->size,_aac_fp);

            }
            LogInfo("pcmcallback packet->pts:%ld",packet->pts);
        //        av_packet_free(&packet);
            _rtsp_push->pushData(packet,E_AUDIO_TYPE);
        }
    }
}


void PushWork::PcmCallback(uint8_t *pcm, int32_t size)
{
//    LogInfo("PcmCallback size:%d",size);
    int resample_nb = 0;
    int frame_bytes1 = 1024*2*4;
    int frame_bytes2 = 0;
    //手动把s16转成fltp
    if(!_fltp_buf)
    {
        _fltp_buf = new uint8_t[frame_bytes1];
    }
    int ret = 0;
    if(!_pcm_s16le_fp)
    {
        _pcm_s16le_fp = fopen("push_dump_s16le.pcm", "wb");
    }
    if(_pcm_s16le_fp)
    {
        // ffplay -ar 48000 -channels 2 -f s16le  -i push_dump_s16le.pcm
        fwrite(pcm, 1, size, _pcm_s16le_fp);
        fflush(_pcm_s16le_fp);
    }

//    LogInfo("PcmCallback size:%d", size);

    if((resample_nb=_audio_resample->ResampleStartS16ToFltp(pcm,_audio_frame))<0)
    {
        LogError("resample fail");
        return;
    }
//    LogInfo("setnb_samples:%d,resample nb_samples:%d",_audio_encoder->getFrameSamples(),resample_nb);
//    s16le_to_fltp((short*)pcm,(float*)_fltp_buf,_audio_frame->nb_samples);
    ret = av_frame_make_writable(_audio_frame);
    if(ret<0)
    {
        LogError("frame cannot write");
        return;
    }

    //距离一开始为0的时间戳差值
    int64_t pts = (int64_t)AVPublishTime::GetInstance()->get_audio_pts();
    RET_CODE encode_ret = RET_OK;
    encode_ret = _audio_encoder->Encode(_audio_frame,pts,0);

    if(encode_ret == RET_OK)
    {
        //接受Packet 并且写入数据到文件和发送至rtsp服务器中
        getAndPutAudioAVPacket();
    }

}
void PushWork::getAndPutVideoAVPacket()
{
    while(1)
    {
        AVPacket *packet = av_packet_alloc();
        int ret1 = avcodec_receive_packet(_video_ctx,packet);
        if(ret1<0)
        {
//            LogError("AAC:avcodec_receive_packet ret:%d",ret1);
            av_packet_free(&packet);
            if(ret1 == AVERROR(EAGAIN))     //需要继续发送才有packet读取
            {
                LogWarn("receive video packet is null");
                break;
            }else if(ret1 == AVERROR_EOF)
            {
                LogWarn("receive video packet is null");     //不能在读取packet来了
                break;
            }else
            {
                LogWarn("receive video packet is null");        //真正报错，销毁encoder
                break;
            }
        }else {
            if(!_h264_fp)
            {
                _h264_fp = fopen("push_dump.h264","wb");
                if(!_h264_fp)
                {
                    LogError("fopen push_dump.h264 failed");
                    return;
                }
                uint8_t start_code[] = {0,0,0,1};
                //写入sps和pps 这两个不带startcode
                fwrite(start_code,1,4,_h264_fp);
                fwrite(_video_encoder->get_sps_data(),1,_video_encoder->get_sps_size(),_h264_fp);
                fwrite(start_code,1,4,_h264_fp);
                fwrite(_video_encoder->get_pps_data(),1,_video_encoder->get_pps_size(),_h264_fp);
            }
            fwrite(packet->data,1,packet->size,_h264_fp);
            //fflush 强制将缓冲区中的数据写入磁盘，确保文件流与实际文件同步。
            fflush(_h264_fp);
    //        av_packet_free(&packet);
            LogInfo("YuvCallback packet->pts:%ld", packet->pts);

            _rtsp_push->pushData(packet,E_VIDEO_TYPE);
    //        LogInfo("YUVCallback pts:%ld",pts);
        }
    }
}
void PushWork::YUVCallback(uint8_t *yuv, int32_t size)
{
//    LogInfo("YUVCallback size:%d",size);
    int64_t pts = (int64_t)AVPublishTime::GetInstance()->get_video_pts();
    RET_CODE encode_ret = RET_OK;
    //virtual AVPacket *Encode(uint8_t *yuv,int size,const int64_t pts,int *pkt_frame,RET_CODE *ret);
    encode_ret = _video_encoder->Encode(yuv,size,pts);
    if(encode_ret == RET_OK)
    {
       getAndPutVideoAVPacket();
    }
}



void PushWork::DeInit()
{
    if(_audio_capturer)
    {
        _audio_capturer->Stop();
        delete _audio_capturer;
        _audio_capturer = NULL;
    }
    if(_video_capturer)
    {
        _video_capturer->Stop();
        delete _video_capturer;
        _video_capturer = NULL;
    }
    if(_audio_resample)
    {
        delete _audio_resample;
        _audio_resample = NULL;
    }
    if(_audio_encoder)
    {
        delete _audio_encoder;
        _audio_encoder = NULL;
    }
    if(_fltp_buf)
        av_free(_fltp_buf);
    if(_audio_frame)
        av_frame_free(&_audio_frame);
    if(_video_encoder)
    {
        delete _video_encoder;
        _video_encoder = NULL;
    }
    if(_rtsp_push)
    {
        delete _rtsp_push;
        _rtsp_push = NULL;
    }
    if(_pcm_s16le_fp)
    {
        fclose(_pcm_s16le_fp);
    }
    if(_h264_fp)
    {
        fclose(_h264_fp);
    }
}
