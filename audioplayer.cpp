#include "audioplayer.h"

void AudioPlayer::startRecord(UINT nChannel, UINT bitDepth, UINT sampleRate, UINT deviceID){
    if (isRecording) {
        qDebug() << "is recording.";
        return;
    }
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
    for (int i = 0; i < WAVEHDR_NUM; ++i) {
        WAVEHDR* waveHeader = new WAVEHDR();
        waveHeader->lpData = new char[WAVEHDR_SIZE];
        waveHeader->dwBufferLength = WAVEHDR_SIZE;
        waveHeader->dwBytesRecorded = 0;
        waveHeader->dwUser = 0;
        waveHeader->dwFlags = 0;
        waveHeader->dwLoops = 0;

        waveInPrepareHeader(this->hWaveIn, waveHeader, sizeof(WAVEHDR));
        waveInAddBuffer(this->hWaveIn, waveHeader, sizeof(WAVEHDR));
        waveHeaders.push_back(waveHeader);
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
    if (!isRecording) {
        qDebug() << "not recording.";
        return;
    }

    if (isPausing) {
        qDebug() << "is pausing.";
        return;
    }

    // 记录暂停时间
    this->pauseTime = QTime::currentTime();
    this->isPausing = true;
    waveInStop(this->hWaveIn);
}

void AudioPlayer::continueRecord(){
    if (!isRecording) {
        qDebug() << "not recording.";
        return;
    }

    if (!isPausing) {
        qDebug() << "not pausing.";
        return;
    }

    // 开始时间加上暂停时间间隔
    this->startTime = this->startTime.addMSecs(this->pauseTime.msecsTo(QTime::currentTime()));
    this->isPausing = false;
    waveInStart(this->hWaveIn);
}

void AudioPlayer::wirteWaveHeader(UINT sampleRate, UINT bitDepth, UINT nChannel){
    // WAV 文件头
    WAVHeader header;

    header.NumChannels = nChannel;
    header.SampleRate = sampleRate;
    header.BitsPerSample = bitDepth;
    header.ByteRate = sampleRate * nChannel * bitDepth / 8;
    header.BlockAlign = nChannel * bitDepth / 8;

    const char* pHeader = reinterpret_cast<const char*>(&header);
    // 写入 WAV 文件头
    this->buffer.insert(this->buffer.begin(), pHeader, pHeader + sizeof(header));
}

WAVEFORMATEX AudioPlayer::readWaveHeader(){
    WAVHeader header;
    // 读取头信息
    this->ifs.read(reinterpret_cast<char*>(&header), sizeof(WAVHeader));

    WAVEFORMATEX waveFormat;
    waveFormat.wFormatTag = header.AudioFormat;  // PCM 格式标志
    waveFormat.nChannels = header.NumChannels;
    waveFormat.nSamplesPerSec = header.SampleRate;
    waveFormat.wBitsPerSample = header.BitsPerSample;
    waveFormat.nBlockAlign = header.BlockAlign;  // 每个采样块的字节数
    waveFormat.nAvgBytesPerSec = header.ByteRate; // 每秒的平均字节数
    waveFormat.cbSize = 0;                    // 无附加信息

    return waveFormat;
}

void CALLBACK AudioPlayer::waveInProc(
    HWAVEIN hwi,
    UINT uMsg,
    DWORD_PTR dwInstance,
    DWORD_PTR dwParam1,
    DWORD_PTR dwParam2){

    // 处理音频数据
    if (uMsg == WIM_DATA) {
        PWAVEHDR waveHeader = reinterpret_cast<PWAVEHDR>(dwParam1);
        AudioPlayer* audioplayer = reinterpret_cast<AudioPlayer*>(dwInstance);

        // 写入数据到文件
        audioplayer->buffer.insert(audioplayer->buffer.end(),
                                   waveHeader->lpData, waveHeader->lpData + waveHeader->dwBytesRecorded);
        waveInAddBuffer(hwi, waveHeader, sizeof(WAVEHDR)); //buffer重新放入采集队列
    }
}

void AudioPlayer::waveOutProc(
    HWAVEOUT hwo,
    UINT uMsg,
    DWORD_PTR dwInstance,
    DWORD_PTR dwParam1,
    DWORD_PTR dwParam2){
}

void AudioPlayer::stopRecord(){
    if (!isRecording) {
        qDebug() << "Not recording.";
        return;
    }
    this->isRecording = false;
    this->isPausing = false;

    if (this->hWaveIn != nullptr) {
        // 停止录制
        waveInStop(this->hWaveIn);

        // 清理录音缓冲区
        for (auto waveHeader : this->waveHeaders) {
            waveInUnprepareHeader(this->hWaveIn, waveHeader, sizeof(WAVEHDR));
            delete[] waveHeader->lpData;
            delete waveHeader;
        }
        this->waveHeaders.clear();

        // 关闭音频输入设备
        waveInClose(this->hWaveIn);
        this->hWaveIn = nullptr; // 将指针置空
    }
}

void AudioPlayer::startPlay(QString& fileName, UINT deviceID){
    this->ifs.open(fileName.toStdString(), std::ios::binary);
    if (!this->ifs.is_open()) {
        qDebug() << "open file error";
        return;
    }

    WAVEFORMATEX waveFormat = readWaveHeader();

    // 打开 WaveOut 设备
    if (waveOutOpen(&this->hWaveOut, deviceID, &waveFormat,
                    reinterpret_cast<DWORD_PTR>(&AudioPlayer::waveInProc),
                    reinterpret_cast<DWORD_PTR>(this), CALLBACK_FUNCTION) != MMSYSERR_NOERROR) {
        qDebug() << "Failed to open wave out device";
        return;
    }

}

void AudioPlayer::pausePlay(){

}

void AudioPlayer::continuePlay(){

}

void AudioPlayer::stopPlay(){

}

void AudioPlayer::saveWaveFile(QString &fileName){
    ofs.open(fileName.toStdString(), std::ios::binary);
    if (!ofs.is_open()) {
        qDebug() << "open file error";
        return;
    }

    // 计算并更新 WAV 文件头中的数据大小信息
    this->ofs.write(reinterpret_cast<const char*>(this->buffer.data()), this->buffer.size());
    int dataSize = static_cast<int>(this->ofs.tellp()) - sizeof(WAVHeader); // 文件大小减去头部44字节
    this->ofs.seekp(40, std::ios::beg); // 移动到 Subchunk2Size 的位置
    this->ofs.write(reinterpret_cast<const char*>(&dataSize), 4); // 更新 Subchunk2Size

    this->ofs.close();
    this->ofs.clear();
}

void AudioPlayer::clearBuffer(){
    // 收缩空间
    std::vector<BYTE>(BUFFER_SIZE).swap(this->buffer);
    this->buffer.clear();
}


AudioPlayer::AudioPlayer(QObject *parent)
    : QObject{parent}{
    this->hWaveIn = nullptr;
    this->waveHeaders.reserve(WAVEHDR_NUM);
    this->buffer.reserve(BUFFER_SIZE);
}

AudioPlayer::~AudioPlayer(){
}
