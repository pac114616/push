#include "commonlooper.h"
#include "dlog.h"
CommonLooper::CommonLooper():
    request_abort(false),
    _running(false)
{

}
void *CommonLooper::trampoline(void *p)
{
    LogInfo("into");
    ((CommonLooper *)p)->SetRunning(true);
    ((CommonLooper *)p)->Loop();
    ((CommonLooper *)p)->SetRunning(false);
    LogInfo("leave");
    return NULL;
}
CommonLooper::~CommonLooper()
{
    Stop();
}

RET_CODE CommonLooper::Start()
{
    LogInfo("CommonLooper::Start into");
    _worker = new std::thread(trampoline,this);
    if(!_worker)
    {
        LogError("new std::this_thread failed");
        return RET_FAIL;
    }
    return RET_OK;
}

void CommonLooper::Stop()
{
    request_abort = true;
    if(_worker)
    {
        _worker->join();//退出线程
        delete _worker;
        _worker = NULL;
    }

}

bool CommonLooper::GetRunning()
{
    return _running;
}

void CommonLooper::SetRunning(bool running)
{
    _running = running;
}



void CommonLooper::Loop()
{

}
