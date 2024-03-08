#include "audioplayer.h"

bool AudioPlayer::startRecord(UINT nChannel, UINT bitDepth, UINT sampleRate, UINT deviceID){
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
        return false;
    }

    // 3. 准备缓冲区
    this->recordBlockSize = waveFormat.nAvgBytesPerSec * BASE_SECOND;
    for (int i = 0; i < RECORD_WAVEHDR_NUM; ++i) {
        WAVEHDR* waveHeader = new WAVEHDR();
        ZeroMemory(waveHeader, sizeof(WAVEHDR));
        waveHeader->lpData = new char[this->recordBlockSize];
        ZeroMemory(waveHeader->lpData, this->recordBlockSize);
        waveHeader->dwBufferLength = this->recordBlockSize;

        waveInPrepareHeader(this->hWaveIn, waveHeader, sizeof(WAVEHDR));
        waveInAddBuffer(this->hWaveIn, waveHeader, sizeof(WAVEHDR));
        recordWaveHeaders.push_back(waveHeader);
    }

    // 4. 写入wav文件头
    wirteWaveHeader(sampleRate, bitDepth, nChannel);

    // 5. 开始录制
    if (waveInStart(this->hWaveIn) != MMSYSERR_NOERROR) {
        qDebug() << "Failed to start record";
        return false;
    }
    // 开始录制
    this->isRecording = true;

    return true;
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
        waveInStop(this->hWaveIn);
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

        // 清理录音缓冲区
        for (auto waveHeader: this->recordWaveHeaders) {
            waveInUnprepareHeader(this->hWaveIn, waveHeader, sizeof(WAVEHDR));
            delete[] waveHeader->lpData;
            delete waveHeader;
        }
        this->recordWaveHeaders.clear();

        // 关闭音频输入设备
        waveInClose(this->hWaveIn);

        // 相关成员置空
        this->recordBlockSize = 0;
        this->hWaveIn = nullptr;
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
    }
}

void AudioPlayer::saveWaveFile(QString &fileName){
    ofs.open(fileName.toStdString(), std::ios::binary | std::ios::trunc);
    if (!ofs.is_open()) {
        qDebug() << "open file error";
        return;
    }

    // 计算并更新 WAV 文件头中的数据大小信息
    this->ofs.write(reinterpret_cast<const char*>(this->recordBuffer.data()), this->recordBuffer.size());
    uint32_t dataSize = static_cast<uint32_t>(this->ofs.tellp()) - sizeof(WAVFileHeader); // 文件大小减去头部44字节
    this->ofs.seekp(40, std::ios::beg); // 移动到 Subchunk2Size 的位置
    this->ofs.write(reinterpret_cast<const char*>(&dataSize), 4); // 更新 Subchunk2Size
    this->ofs.seekp(4, std::ios::beg); // 移动到 ChunkSize 的位置
    uint32_t chunkSize = sizeof(WAVFileHeader) - 8 + dataSize;
    this->ofs.write(reinterpret_cast<const char*>(&chunkSize), 4); // 更新 Subchunk2Size

    this->ofs.close();
    this->ofs.clear();
}

void AudioPlayer::clearData(){
    // 清除录制数据
    std::vector<BYTE>(RECORD_BUFFER_SIZE).swap(this->recordBuffer);
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





bool AudioPlayer::startPlay(QString& fileName, UINT deviceID){
    // 0. 记录开始时间
    this->startTime = QTime::currentTime();

    this->ifs.open(fileName.toStdString(), std::ios::binary);
    if (!this->ifs.is_open()) {
        qDebug() << "open file error";
        return false;
    }

    WAVEFORMATEX waveFormat = readWaveHeader();
    // 打开 WaveOut 设备
    MMRESULT res = waveOutOpen(&this->hWaveOut, deviceID, &waveFormat,
                reinterpret_cast<DWORD_PTR>(&AudioPlayer::waveOutProc),
                reinterpret_cast<DWORD_PTR>(this),
                               CALLBACK_FUNCTION);
    if (res != MMSYSERR_NOERROR) { // MMSYSERR_INVALPARAM
        qDebug() << "Failed to open wave out device";
        return false;
    }

    // 准备双缓冲区
    this->playBlockSize = waveFormat.nAvgBytesPerSec * BASE_SECOND; // 4s的数据量
    this->playWaveHeader_1 = new WAVEHDR();
    ZeroMemory(this->playWaveHeader_1, sizeof(WAVEHDR));
    this->playWaveHeader_1->lpData = new char[this->playBlockSize];
    this->playWaveHeader_1->dwBufferLength = this->playBlockSize;
    waveOutPrepareHeader(this->hWaveOut, this->playWaveHeader_1, sizeof(WAVEHDR));

    this->playWaveHeader_2 = new WAVEHDR();
    ZeroMemory(this->playWaveHeader_2, sizeof(WAVEHDR));
    this->playWaveHeader_2->lpData = new char[this->playBlockSize];
    this->playWaveHeader_2->dwBufferLength = this->playBlockSize;
    waveOutPrepareHeader(this->hWaveOut, this->playWaveHeader_2, sizeof(WAVEHDR));

    // 填充第一块数据
    this->ifs.read(this->playWaveHeader_1->lpData, this->playBlockSize);
    this->playWaveHeader_1->dwBytesRecorded = this->ifs.gcount();
    // 填充第二块数据
    this->ifs.read(this->playWaveHeader_2->lpData, this->playBlockSize);
    this->playWaveHeader_2->dwBytesRecorded = this->ifs.gcount();

    /*
    问题： 缓冲区切换时会有轻微卡顿
    解决要点： 保证播放队列中时刻都有一个缓冲区，
    这里先将两块数据都加入队列， 然后在回调函数中填充播放完毕的缓冲区并再次加入队列， 即可完成流畅播放
    */
    // 将两个缓冲区加入播放队列
    waveOutWrite(this->hWaveOut, this->playWaveHeader_1, sizeof(WAVEHDR));
    waveOutWrite(this->hWaveOut, this->playWaveHeader_2, sizeof(WAVEHDR));

    // 开始播放
    this->isPlaying = true;
    return true;
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
        // 清理播放缓冲区
        waveOutUnprepareHeader(this->hWaveOut, this->playWaveHeader_1, sizeof(WAVEHDR));
        delete[] this->playWaveHeader_1->lpData;
        delete this->playWaveHeader_1;

        waveOutUnprepareHeader(this->hWaveOut, this->playWaveHeader_2, sizeof(WAVEHDR));
        delete[] this->playWaveHeader_2->lpData;
        delete this->playWaveHeader_2;

        // 关闭音频输入设备
        waveOutClose(this->hWaveOut);

        // 相关成员置空
        if (this->ifs.is_open()) {
            this->ifs.close();
            this->ifs.clear();
        }
        this->playWaveHeader_1 = nullptr;
        this->playWaveHeader_2 = nullptr;

        this->hWaveOut = nullptr;
        this->playBlockSize = 0;
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
        if (uMsg == WOM_DONE) {
            PWAVEHDR used = reinterpret_cast<PWAVEHDR>(dwParam1);

            // 重新填充
            audioplayer->ifs.read(used->lpData, audioplayer->playBlockSize);
            used->dwBytesRecorded = audioplayer->ifs.gcount();
            used->dwFlags &= ~WHDR_DONE; // 清除 WHDR_DONE 标志位，表示数据已经填充

            // 重新加入播放队列
            waveOutWrite(hwo, used, sizeof(WAVEHDR));

            // 检查是否所有缓冲区都已经播放完毕，如果是，则停止播放
            if ((audioplayer->playWaveHeader_1->dwFlags & WHDR_DONE) && (audioplayer->playWaveHeader_2->dwFlags & WHDR_DONE)) {
                audioplayer->stopPlay();
            }
        }
    }
}

WAVEFORMATEX AudioPlayer::readWaveHeader(){
    WAVFileHeader header;
    ZeroMemory(&header, sizeof(WAVFileHeader));
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

    // 视频时长(ms), 为了防止溢出, 需要先转换为uint64_t进行计算
    this->audioDuration = static_cast<uint64_t>(header.Subchunk2Size) * 1000 / header.ByteRate;
    return waveFormat;
}

AudioPlayer::AudioPlayer(QObject *parent)
    : QObject{parent}{
    this->hWaveIn = nullptr;
    this->hWaveOut = nullptr;

    this->recordBlockSize = 0;
    this->playBlockSize = 0;

    this->playWaveHeader_1 = nullptr;
    this->playWaveHeader_2 = nullptr;

    this->recordWaveHeaders.reserve(RECORD_WAVEHDR_NUM);
    this->recordBuffer.reserve(RECORD_BUFFER_SIZE);
}

AudioPlayer::~AudioPlayer(){
}
