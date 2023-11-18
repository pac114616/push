#ifndef AUDIOCAPTURE_H
#define AUDIOCAPTURE_H
#include <functional>
#include "mediabase.h"
#include <iostream>
#include "timesutil.h"
#include "commonlooper.h"
#include "avpublishtime.h"
class AudioCapture:public CommonLooper
{
public:
    AudioCapture();
    virtual ~AudioCapture();
    RET_CODE Init(const Properties properties);

    virtual void Loop();
    void AddCallback(std::function<void(uint8_t *,int32_t)> callback);
private:

    // PCM file只是用来测试, 写死为s16格式 2通道 采样率48Khz
    // 1帧1024采样点持续的时间21.333333333333333333333333333333ms
    int openPcmFile(const char *file_name);
    int readPcmFile(uint8_t *pcm_buf, int32_t pcm_buf_size);
    int closePcmFile();

    int _audio_test = 0;
    std::string _input_pcm_name;  //输入音频文件名
    std::string _input_yuv_name;  //输入yuv文件名
    FILE *_pcm_fp = NULL;
    int64_t _pcm_start_time = 0;
    double _pcm_total_duration = 0; //推流时长统计
    double _frame_duration = 23.2;
    std::function<void(uint8_t *,int32_t)> _callback_get_pcm;
    bool _is_first_time = false;
    int _sample_rate = 48000;
    int _nb_samples = 1024;
    int _format = 1;    //目前固定s16
    int _channels = 2;
    int _byte_per_sample = 2;
    uint8_t *_pcm_buf;
    int32_t _pcm_buf_size;
};

#endif // AUDIOCAPTURE_H
