#include "audioplayer.h"

void AudioPlayer::startRecord(UINT nChannel, UINT bitDepth, UINT sampleRate, UINT deviceID){
    this->isRecording = true; // 开始录制

    // 0. 记录开始时间
    this->startTime = QTime::currentTime();
    // 1. 设置音频格式
    WAVEFORMATEX waveFormat;
    waveFormat.wFormatTag = WAVE_FORMAT_PCM;  // PCM 格式标志
    waveFormat.nChannels = nChannel;
    waveFormat.nSamplesPerSec = sampleRate;
    waveFormat.wBitsPerSample = bitDepth;
    waveFormat.nBlockAlign = waveFormat.nChannels * waveFormat.wBitsPerSample / 8;  // 每个采样块的字节数
    waveFormat.nAvgBytesPerSec = waveFormat.nSamplesPerSec * waveFormat.nBlockAlign; // 每秒的平均字节数
    waveFormat.cbSize = 0;                    // 无附加信息

    // 2. 打开音频设备, 同时传递对象指针, 方便回调函数使用
    if (waveInOpen(&this->hWaveIn, deviceID, &waveFormat,
                   reinterpret_cast<DWORD_PTR>(&AudioPlayer::waveInProc),
                   reinterpret_cast<DWORD_PTR>(this), CALLBACK_FUNCTION) != MMSYSERR_NOERROR) {
        qDebug() << "Failed to open wave in device";
        return;
    }

    // 3. 准备缓冲区
    for (int i = 0; i < RECORD_WAVEHDR_NUM; ++i) {
        WAVEHDR* waveHeader = new WAVEHDR();
        ZeroMemory(waveHeader, sizeof(WAVEHDR));
        waveHeader->lpData = new char[WAVEHDR_SIZE];
        waveHeader->dwBufferLength = WAVEHDR_SIZE;

        waveInPrepareHeader(this->hWaveIn, waveHeader, sizeof(WAVEHDR));
        waveInAddBuffer(this->hWaveIn, waveHeader, sizeof(WAVEHDR));
        recordWaveHeaders.push_back(waveHeader);
    }

    // 4. 写入wav文件头
    wirteWaveHeader(sampleRate, bitDepth, nChannel);

    // 5. 开始录制
    if (waveInStart(this->hWaveIn) != MMSYSERR_NOERROR) {
        qDebug() << "Failed to start record";
        return;
    }
}

void AudioPlayer::pauseRecord(){
    // 记录暂停时间
    this->pauseTime = QTime::currentTime();
    this->isPausing = true;
    waveInStop(this->hWaveIn);
}

void AudioPlayer::continueRecord(){
    // 开始时间加上暂停时间间隔
    this->startTime = this->startTime.addMSecs(this->pauseTime.msecsTo(QTime::currentTime()));
    this->isPausing = false;
    waveInStart(this->hWaveIn);
}

void AudioPlayer::stopRecord(){
    this->isRecording = false;
    this->isPausing = false;

    if (this->hWaveIn != nullptr) {
        // 停止录制
        MMRESULT res;
        res = waveInStop(this->hWaveIn);
        qDebug() << "stopRecord-waveInStop: return code " << res;
        /*
        官方文档: waveInReset 函数停止给定波形音频输入设备上的输入，并将当前位置重置为零。
                所有挂起的缓冲区都标记为已完成并返回到应用程序。
        原因: 在停止录音时，要调用waveInReset，这个函数的机制是：
                将队列中剩下的buff（此时的buff很可能并没有填充满）发送到回调函数的WIM_DATA，
                而WIM_DATA中如果执行到waveInAddBuffer这一步，就会把这个buff又放到队列中，
                从而产生死锁，无法正常停止录音。
        解决办法: WIM_DATA中判断isRecording, 若是, 则读取数据后将缓冲区重新加入缓冲区, 否则读取数据后直接跳出
        * */
        waveInReset(this->hWaveIn);
        // 关闭音频输入设备
        res = waveInClose(this->hWaveIn);
        qDebug() << "stopRecord-waveInClose: return code " << res;
    }
}

void CALLBACK AudioPlayer::waveInProc(
    HWAVEIN hwi,
    UINT uMsg,
    DWORD_PTR dwInstance,
    DWORD_PTR dwParam1,
    DWORD_PTR dwParam2){

    AudioPlayer* audioplayer = reinterpret_cast<AudioPlayer*>(dwInstance);
    // 处理音频数据
    switch (uMsg) {
    case WIM_DATA:{
        PWAVEHDR waveHeader = reinterpret_cast<PWAVEHDR>(dwParam1);

        // 写入数据到文件
        audioplayer->recordBuffer.insert(audioplayer->recordBuffer.end(),
                                         waveHeader->lpData, waveHeader->lpData + waveHeader->dwBytesRecorded);
        // 解决死锁, 详见waveInReset调用处
        if (audioplayer->isRecording) {
            waveInAddBuffer(hwi, waveHeader, sizeof(WAVEHDR)); //buffer重新放入采集队列
        }
        break;
    }
    case WIM_OPEN: {
        break;
    }
    case WIM_CLOSE: {
        // 清理录音缓冲区
        for (auto waveHeader: audioplayer->recordWaveHeaders) {
            waveInUnprepareHeader(hwi, waveHeader, sizeof(WAVEHDR));
            delete[] waveHeader->lpData;
            delete waveHeader;
        }
        audioplayer->recordWaveHeaders.clear();
        audioplayer->hWaveIn = nullptr;
        break;
    }
    }
}

void AudioPlayer::saveWaveFile(QString &fileName){
    ofs.open(fileName.toStdString(), std::ios::binary);
    if (!ofs.is_open()) {
        qDebug() << "open file error";
        return;
    }

    // 计算并更新 WAV 文件头中的数据大小信息
    this->ofs.write(reinterpret_cast<const char*>(this->recordBuffer.data()), this->recordBuffer.size());
    int dataSize = static_cast<int>(this->ofs.tellp()) - sizeof(WAVFileHeader); // 文件大小减去头部44字节
    this->ofs.seekp(40, std::ios::beg); // 移动到 Subchunk2Size 的位置
    this->ofs.write(reinterpret_cast<const char*>(&dataSize), 4); // 更新 Subchunk2Size
    this->ofs.seekp(4, std::ios::beg); // 移动到 ChunkSize 的位置
    int chunkSize = sizeof(WAVFileHeader) - 8 + dataSize;
    this->ofs.write(reinterpret_cast<const char*>(&chunkSize), 4); // 更新 Subchunk2Size

    this->ofs.close();
    this->ofs.clear();
}

void AudioPlayer::clearRecordBuffer(){
    // 收缩空间
    std::vector<BYTE>(BUFFER_SIZE).swap(this->recordBuffer);
    this->recordBuffer.clear();
}

void AudioPlayer::wirteWaveHeader(UINT sampleRate, UINT bitDepth, UINT nChannel){
    // WAV 文件头
    WAVFileHeader header;

    header.NumChannels = nChannel;
    header.SampleRate = sampleRate;
    header.BitsPerSample = bitDepth;
    header.ByteRate = sampleRate * nChannel * (bitDepth / 8);
    header.BlockAlign = nChannel * (bitDepth / 8);

    const char* pHeader = reinterpret_cast<const char*>(&header);
    this->recordBuffer.insert(this->recordBuffer.begin(), pHeader, pHeader + sizeof(WAVFileHeader));
}





void AudioPlayer::startPlay(QString& fileName, UINT deviceID){
    this->isPlaying = true; // 开始播放

    // 0. 记录开始时间
    this->startTime = QTime::currentTime();

    this->ifs.open(fileName.toStdString(), std::ios::binary);
    if (!this->ifs.is_open()) {
        qDebug() << "open file error";
        return;
    }

    WAVEFORMATEX waveFormat = readWaveHeader();
    // 打开 WaveOut 设备
    // 第二次打开失败
    if (waveOutOpen(&this->hWaveOut, deviceID, &waveFormat,
                    reinterpret_cast<DWORD_PTR>(&AudioPlayer::waveOutProc),
                    reinterpret_cast<DWORD_PTR>(this),
                    CALLBACK_FUNCTION) != MMSYSERR_NOERROR) {
        qDebug() << "Failed to open wave out device";
        return;
    }

    // 准备双缓冲区
    this->playWaveHeader_1 = new WAVEHDR();
    ZeroMemory(this->playWaveHeader_1, sizeof(WAVEHDR));
    this->playWaveHeader_1->lpData = new char[WAVEHDR_SIZE];
    this->playWaveHeader_1->dwBufferLength = WAVEHDR_SIZE;
    waveOutPrepareHeader(this->hWaveOut, this->playWaveHeader_1, sizeof(WAVEHDR));

    this->playWaveHeader_2 = new WAVEHDR();
    ZeroMemory(this->playWaveHeader_2, sizeof(WAVEHDR));
    this->playWaveHeader_2->lpData = new char[WAVEHDR_SIZE];
    this->playWaveHeader_2->dwBufferLength = WAVEHDR_SIZE;
    waveOutPrepareHeader(this->hWaveOut, this->playWaveHeader_2, sizeof(WAVEHDR));
    // 当前缓冲区设置为一号
    this->currPlayWaveHeader = this->playWaveHeader_1;

    // 填充第一块数据
    this->ifs.read(this->playWaveHeader_1->lpData, WAVEHDR_SIZE);
    this->playWaveHeader_1->dwBytesRecorded = this->ifs.gcount();
    // 填充第二块数据
    this->ifs.read(this->playWaveHeader_2->lpData, WAVEHDR_SIZE);
    this->playWaveHeader_2->dwBytesRecorded = this->ifs.gcount();
    // 播放当前块
    waveOutWrite(this->hWaveOut, this->playWaveHeader_1, sizeof(WAVEHDR));
}

void AudioPlayer::pausePlay(){
    // 记录暂停时间
    this->pauseTime = QTime::currentTime();
    this->isPausing = true;
    waveOutPause(this->hWaveOut);
}

void AudioPlayer::continuePlay(){
    // 开始时间加上暂停时间间隔
    this->startTime = this->startTime.addMSecs(this->pauseTime.msecsTo(QTime::currentTime()));
    this->isPausing = false;
    waveOutRestart(this->hWaveOut);
}

void AudioPlayer::stopPlay(){
    this->isPlaying = false;
    this->isPausing = false;

    if (this->hWaveOut != nullptr) {
        // 停止录制
        waveOutReset(this->hWaveOut);

        // 关闭音频输入设备
        MMRESULT res = waveOutClose(this->hWaveOut);
        qDebug() << "stopPlay-waveOutClose: return code " << res;
    }
}

void AudioPlayer::waveOutProc(
    HWAVEOUT hwo,
    UINT uMsg,
    DWORD_PTR dwInstance,
    DWORD_PTR dwParam1,
    DWORD_PTR dwParam2){

    AudioPlayer* audioplayer = reinterpret_cast<AudioPlayer*>(dwInstance);
    // 解决死锁, 停止播放后不写入数据
    if (audioplayer->isPlaying) {
        switch (uMsg) {
        case WOM_DONE: {
            PWAVEHDR playedWaveHeader = reinterpret_cast<PWAVEHDR>(dwParam1);
            // 填充已使用块
            audioplayer->ifs.read(playedWaveHeader->lpData, WAVEHDR_SIZE);
            // 记录预填充块大小, 下次回调再检查是否有数据
            playedWaveHeader->dwBytesRecorded = audioplayer->ifs.gcount();

            // 切换下一个待播放数据块
            audioplayer->currPlayWaveHeader = (playedWaveHeader == audioplayer->playWaveHeader_1) ?

                audioplayer->playWaveHeader_2 : audioplayer->playWaveHeader_1;
            // 若下一个待播放数据块无数据, 则结束播放
            if (audioplayer->currPlayWaveHeader->dwBytesRecorded <= 0) {
                audioplayer->stopPlay();
                return;
            }
            // 播放下一块
            waveOutWrite(hwo, audioplayer->currPlayWaveHeader, sizeof(WAVEHDR));

            break;
        }
        case WOM_OPEN: {
            break;
        }
        case WOM_CLOSE:{
            // 清理播放缓冲区
            waveOutUnprepareHeader(hwo, audioplayer->playWaveHeader_1, sizeof(WAVEHDR));
            delete[] audioplayer->playWaveHeader_1->lpData;
            delete audioplayer->playWaveHeader_1;

            waveOutUnprepareHeader(hwo, audioplayer->playWaveHeader_2, sizeof(WAVEHDR));
            delete[] audioplayer->playWaveHeader_2->lpData;
            delete audioplayer->playWaveHeader_2;

            audioplayer->currPlayWaveHeader = nullptr;
            audioplayer->playWaveHeader_1 = nullptr;
            audioplayer->playWaveHeader_2 = nullptr;

            audioplayer->hWaveOut = nullptr;
            break;
        }
        }
    }
}

WAVEFORMATEX AudioPlayer::readWaveHeader(){
    WAVFileHeader header;
    // 读取头信息
    this->ifs.read(reinterpret_cast<char*>(&header), sizeof(WAVFileHeader));

    WAVEFORMATEX waveFormat;
    waveFormat.wFormatTag = header.AudioFormat;  // PCM 格式标志
    waveFormat.nChannels = header.NumChannels;
    waveFormat.nSamplesPerSec = header.SampleRate;
    waveFormat.wBitsPerSample = header.BitsPerSample;
    waveFormat.nBlockAlign = header.BlockAlign;  // 每个采样块的字节数
    waveFormat.nAvgBytesPerSec = header.ByteRate; // 每秒的平均字节数
    waveFormat.cbSize = 0;                    // 无附加信息

    // 视频时长(ms): 数据部分大小 * 1000 / 每秒比特率
    this->audioDuration = header.Subchunk2Size * 1000 / header.ByteRate;
    return waveFormat;
}

AudioPlayer::AudioPlayer(QObject *parent)
    : QObject{parent}{
    this->hWaveIn = nullptr;
    this->hWaveOut = nullptr;

    this->currPlayWaveHeader = nullptr;
    this->playWaveHeader_1 = nullptr;
    this->playWaveHeader_2 = nullptr;

    this->recordWaveHeaders.reserve(RECORD_WAVEHDR_NUM);
    this->recordBuffer.reserve(BUFFER_SIZE);
}

AudioPlayer::~AudioPlayer(){
}
