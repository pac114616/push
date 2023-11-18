#include "audiocapture.h"
#include "dlog.h"
extern "C" {

#include <libswresample/swresample.h>
}
AudioCapture::AudioCapture():CommonLooper()
{

}

AudioCapture::~AudioCapture()
{
    if(_pcm_buf)
        delete [] _pcm_buf;
    closePcmFile();
}

RET_CODE AudioCapture::Init(const Properties properties)
{
    _audio_test = properties.GetProperty("audio_test",0);
    _input_pcm_name = properties.GetProperty("input_pcm_name","48000_2_s16le.pcm");
    _sample_rate = properties.GetProperty("sample_rate",48000);
    _nb_samples = properties.GetProperty("nb_samples",1024);
    _byte_per_sample = properties.GetProperty("byte_per_sample",AV_SAMPLE_FMT_S16);//单个采样点
    _channels = properties.GetProperty("channels",2);
    _format = properties.GetProperty("format",2);
    _pcm_buf_size = _channels * _nb_samples * _byte_per_sample;
    _pcm_buf = new uint8_t[_pcm_buf_size];
    if(!_pcm_buf)
        return RET_ERR_OUTFMEMORY;
    if(openPcmFile(_input_pcm_name.c_str())!=0)
    {
        LogError("openPcmFile %s failed",_input_pcm_name.c_str());
        return RET_FAIL;
    }
    _frame_duration = _nb_samples*1.0/_sample_rate *1000; //ms

    return RET_OK;
}

void AudioCapture::Loop()
{
    LogInfo("into loop");
    _pcm_total_duration = 0;
    _pcm_start_time = TimesUtil::GetTimeMillisecond();//初始化时间基
    while(1)
    {
        if(request_abort)
            break;
        if(readPcmFile(_pcm_buf,_pcm_buf_size)==0)
        {
            //回调
            if(_is_first_time)
            {
                //第一帧数据debug
                _is_first_time = true;
                LogInfo("%s:t%u", AVPublishTime::GetInstance()->getAInTag(),
                        AVPublishTime::GetInstance()->getCurrenTime());
            }
            //获取一帧数据后进行冲采样等操作
            if(_callback_get_pcm)
                _callback_get_pcm(_pcm_buf,_pcm_buf_size);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    request_abort = false;
    closePcmFile();

}

void AudioCapture::AddCallback(std::function<void (uint8_t *, int32_t)> callback)
{
    //_callback_get_pcm = PcmCallback
    _callback_get_pcm = callback;
}

int AudioCapture::openPcmFile(const char *file_name)
{
    _pcm_fp = fopen(file_name,"rb");
    if(!_pcm_fp)
        return -1;
    return 0;
}

int AudioCapture::readPcmFile(uint8_t *pcm_buf, int32_t pcm_buf_size)
{
    int64_t cur_time = TimesUtil::GetTimeMillisecond(); //单位毫秒 当前时间
    int64_t dif = cur_time - _pcm_start_time;           //目前经过的时间
    if((int64_t)_pcm_total_duration>dif)
        return -1; //还没有到读取新一帧的时间
    //读取数据
    size_t ret = fread(pcm_buf,1,pcm_buf_size,_pcm_fp);
    if(ret!=pcm_buf_size)
    {
        //文件指针 _pcm_fp 定位到文件的开头，以便重新开始读取文件。这是为了处理读取不完整的情况。
        ret = fseek(_pcm_fp,0,SEEK_SET);
        ret = fread(_pcm_buf,1,pcm_buf_size,_pcm_fp);
        if(ret!=pcm_buf_size)
            return -1;//出错
    }
    _pcm_total_duration += _frame_duration;
    return 0;
}

int AudioCapture::closePcmFile()
{
    if(_pcm_fp)
       fclose(_pcm_fp);
    return 0;
}

