#include "audioresample.h"
#include "dlog.h"
AudioResample::AudioResample()
{

}

AudioResample::~AudioResample()
{
    DeInit();
}

void AudioResample::DeInit()
{
    _in_channels = 0;
    _in_sample_rate = 0;
    _in_channel_layout = 0;
    _out_channels = 0;
    _out_sample_rate = 0;
    _out_channel_layout = 0;
    if(_ctx)
    {
        swr_free(&_ctx);
        _ctx = NULL;
    }
}

RET_CODE AudioResample::InitS16ToFltp(int in_channels, int in_samples_rate,
                                 int out_channels,int out_samples_rate)
{
    _in_channels = in_channels;
    _in_channel_layout = av_get_default_channel_layout(in_channels);
    _in_sample_rate = in_samples_rate;
    _out_channels = out_channels;
    _out_channel_layout = av_get_default_channel_layout(out_channels);
    _out_sample_rate = out_samples_rate;

    _ctx = swr_alloc_set_opts(
                            _ctx,
                            _out_channel_layout,
                            AV_SAMPLE_FMT_FLTP,
                            _out_sample_rate,
                            _in_channel_layout,
                            AV_SAMPLE_FMT_S16,
                            _in_sample_rate,
                            0,
                            NULL);
    if(!_ctx)
    {
        LogError("swr ctx alloc set opts error");
        return RET_FAIL;
    }
    int ret = swr_init(_ctx);
    if(ret<0)
    {
        char errbuf[1024] = {0};
        av_strerror(ret, errbuf, sizeof(errbuf) - 1);
        printf("swr_init  failed:%s\n",  errbuf);
        return RET_FAIL;
    }
    return RET_OK;
}

int AudioResample::ResampleStartS16ToFltp(uint8_t *in_data, AVFrame *out_frame)
{
    const uint8_t *indata[AV_NUM_DATA_POINTERS] = {0};
    indata[0] = in_data;
    int samples = swr_convert(_ctx, out_frame->data, out_frame->nb_samples,
                              indata, out_frame->nb_samples);
    if(samples<0)
        return -1;
    return samples; //单个通道的采样点
}

RET_CODE AudioResample::allocFLTPFrame(AVFrame *&frame,int samples,int channels,int channel_layout)
{
    if(frame!=nullptr)
    {
        LogWarn("frame already alloc!");
        return RET_FAIL;
    }
    frame = av_frame_alloc();
    frame->nb_samples = samples;
    frame->channels = channels;
    frame->channel_layout = channel_layout;
    frame->format = AV_SAMPLE_FMT_FLTP;
    int ret = av_frame_get_buffer(frame,0);
    if(ret<0)
    {
        LogError("get buffer fail");
        return RET_FAIL;
    }
    return RET_OK;

}

