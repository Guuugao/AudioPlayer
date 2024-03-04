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
    for (int i = 0; i < WAVEHDR_NUM; ++i) {
        WAVEHDR* waveHeader = new WAVEHDR();
        ZeroMemory(waveHeader, sizeof(WAVEHDR));
        waveHeader->lpData = new char[WAVEHDR_SIZE];
        waveHeader->dwBufferLength = WAVEHDR_SIZE;

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
//    // 创建一个 WAV 头部数组并填充信息
//    char header[44];

//    // 填充 ChunkID，文件标识符 "RIFF"
//    std::memcpy(header, "RIFF", 4);

//    // 填充 ChunkSize，暂时设置为0，待文件写入完毕后再修改
//    uint32_t chunkSize = 0;
//    std::memcpy(header + 4, &chunkSize, sizeof(uint32_t));

//    // 填充 Format，文件格式 "WAVE"
//    std::memcpy(header + 8, "WAVE", 4);

//    // 填充 Subchunk1ID，格式块标识符 "fmt "
//    std::memcpy(header + 12, "fmt ", 4);

//    // 填充 Subchunk1Size，格式块大小，固定为 16
//    uint32_t subchunk1Size = 16;
//    std::memcpy(header + 16, &subchunk1Size, sizeof(uint32_t));

//    // 填充 AudioFormat，音频格式，PCM 格式为 1
//    uint16_t audioFormat = 1;
//    std::memcpy(header + 20, &audioFormat, sizeof(uint16_t));

//    // 填充 NumChannels，声道数
//    std::memcpy(header + 22, &nChannel, sizeof(uint16_t));

//    // 填充 SampleRate，采样率
//    std::memcpy(header + 24, &sampleRate, sizeof(uint32_t));

//    // 填充 ByteRate，每秒的数据量
//    uint32_t byteRate = sampleRate * nChannel * (bitDepth / 8);  // 16 bits per sample
//    std::memcpy(header + 28, &byteRate, sizeof(uint32_t));

//    // 填充 BlockAlign，数据块对齐
//    uint16_t blockAlign = nChannel * (bitDepth / 8);  // 16 bits per sample
//    std::memcpy(header + 32, &blockAlign, sizeof(uint16_t));

//    // 填充 BitsPerSample，位深
//    std::memcpy(header + 34, &bitDepth, sizeof(uint16_t));

//    // 填充 Subchunk2ID，数据块标识符 "data"
//    std::memcpy(header + 36, "data", 4);

//    // 填充 Subchunk2Size，音频数据的大小(初始大小为0, 录制完毕后修改)
//    uint32_t subchunk2Size = 0;
//    std::memcpy(header + 40, &subchunk2Size, sizeof(uint32_t));

//    this->recordBuffer.insert(this->recordBuffer.begin(), header, header + sizeof(header));
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

    return waveFormat;
}

void AudioPlayer::playNextBlock(){
    this->ifs.read(this->playBuffer, WAVEHDR_SIZE);
    int bytesRead = this->ifs.gcount();

    if (bytesRead > 0) {
        WAVEHDR waveHeader;
        ZeroMemory(&waveHeader, sizeof(WAVEHDR));
        waveHeader.dwBufferLength = bytesRead;
        waveHeader.lpData = this->playBuffer;

        waveOutPrepareHeader(this->hWaveOut, &waveHeader, sizeof(WAVEHDR));
        waveOutWrite(this->hWaveOut, &waveHeader, sizeof(WAVEHDR));
    } else {
        stopPlay();
    }
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
        audioplayer->recordBuffer.insert(audioplayer->recordBuffer.end(),
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

    // 在上一块音频播放完毕时加载下一块
    if (uMsg == WOM_DONE) {
        AudioPlayer* audioplayer = reinterpret_cast<AudioPlayer*>(dwInstance);
        audioplayer->playNextBlock();
    }
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
//        waveInReset(this->hWaveIn); // 调用该函数导致崩溃

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
                    reinterpret_cast<DWORD_PTR>(this),
                    CALLBACK_FUNCTION) != MMSYSERR_NOERROR) {
        qDebug() << "Failed to open wave out device";
        return;
    }
    // 播放第一块, 后续会自动调用回调函数
    playNextBlock();
}

void AudioPlayer::pausePlay(){
    waveOutPause(this->hWaveOut);
}

void AudioPlayer::continuePlay(){
    waveOutRestart(this->hWaveOut);
}

void AudioPlayer::stopPlay(){
    waveOutReset(this->hWaveOut);
    waveOutClose(this->hWaveOut);
    this->hWaveOut = nullptr;
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


AudioPlayer::AudioPlayer(QObject *parent)
    : QObject{parent}{
    this->hWaveIn = nullptr;
    this->hWaveOut = nullptr;
    this->waveHeaders.reserve(WAVEHDR_NUM);
    this->recordBuffer.reserve(BUFFER_SIZE);
}

AudioPlayer::~AudioPlayer(){
}
