#ifndef AUDIORESAMPLE_H
#define AUDIORESAMPLE_H
#include "mediabase.h"
extern "C"
{
#include "libavcodec/avcodec.h"
#include "libswresample/swresample.h"
#include "libavformat/avformat.h"
#include <libavutil/opt.h>
}
class AudioResample
{
public:
    AudioResample();
    ~AudioResample();
    void DeInit();
    RET_CODE InitS16ToFltp(int in_channels, int in_samples_rate,int out_channels,int out_samples_rate);
    int ResampleStartS16ToFltp(uint8_t *in_data, AVFrame *out_frame);
    RET_CODE allocFLTPFrame(AVFrame *&frame,int samples,int channels,int channel_layout);
private:
    int _in_channels = 0;
    int _in_sample_rate = 0;
    int _in_channel_layout = 0;
    int _out_channels = 0;
    int _out_sample_rate = 0;
    int _out_channel_layout = 0;
    SwrContext *_ctx = NULL;
};

#endif // AUDIORESAMPLE_H
