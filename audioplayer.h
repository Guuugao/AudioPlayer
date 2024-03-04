#ifndef AUDIOPLAYER_H
#define AUDIOPLAYER_H

#include <fstream>
#include <windows.h>
#include <thread>

#include <QDebug>
#include <QObject>
#include <QDir>

class AudioPlayer : public QObject
{
    Q_OBJECT
signals:
public:
    QTime startTime; // 开始录制/播放的时间
    QTime pauseTime; // 暂停操作开始的时间

    bool isRecording = false; // 是否正在录制
    bool isPlaying = false; // 是否正在播放
    bool isPausing = false; // 是否暂停

    void startRecord(UINT nChannel, UINT bitDepth, UINT sampleRate, UINT deviceID); // 开始录制
    void pauseRecord(); // 暂停录制
    void continueRecord(); // 继续录制
    void stopRecord(); // 结束录制

    void startPlay(QString& fileName, UINT deviceID); // 开始播放
    void pausePlay(); // 暂停播放
    void continuePlay(); // 继续播放
    void stopPlay(); // 结束播放

    void saveWaveFile(QString &fileName); // 保存文件
    void clearRecordBuffer(); // 清除缓冲区

    explicit AudioPlayer(QObject *parent = nullptr);
    ~AudioPlayer();
private:
    static constexpr int WAVEHDR_NUM = 4;
    static constexpr int WAVEHDR_SIZE = 1024 * 4; // 4KB
    static constexpr int BUFFER_SIZE = 1024 * 1024; // 1MB

    HWAVEIN hWaveIn;
    HWAVEOUT hWaveOut;

    std::ofstream ofs;
    std::ifstream ifs;
    std::vector<BYTE> recordBuffer; // 录制音频缓冲区
    char playBuffer[WAVEHDR_SIZE]; // 播放音频缓冲区, 用作WAVEHDR.lpData指向的数据块
    std::vector<PWAVEHDR> waveHeaders;

    // 回调处理录制的音频数据
    static void CALLBACK waveInProc(
        HWAVEIN hwi,
        UINT uMsg,
        DWORD_PTR dwInstance,
        DWORD_PTR dwParam1,
        DWORD_PTR dwParam2);
    // 回调处理播放的音频数据
    static void CALLBACK waveOutProc(
        HWAVEOUT hwo,
        UINT uMsg,
        DWORD_PTR dwInstance,
        DWORD_PTR dwParam1,
        DWORD_PTR dwParam2);

    // 写入wave文件头信息
    void wirteWaveHeader(UINT sampleRate, UINT bitDepth, UINT nChannel);
    // 读取wave文件头信息, 返回文件格式信息
    WAVEFORMATEX readWaveHeader();
    // 播放下一块数据
    void playNextBlock();

    /*
     * WAV 文件头结构体
     *
     * ChunkID: 4 字节，标识文件格式，一般为 "RIFF"。
     * ChunkSize: 4 字节，表示文件大小（不包括 ChunkID 和 ChunkSize 本身的大小）。
     * Format: 4 字节，标识文件格式，一般为 "WAVE"。
     * Subchunk1ID: 4 字节，表示子块1标识符，一般为 "fmt "。
     * Subchunk1Size: 4 字节，表示子块1大小，通常为 16。
     * AudioFormat: 2 字节，表示音频格式，PCM 为 1。
     * NumChannels: 2 字节，表示声道数，单声道为 1，立体声为 2。
     * SampleRate: 4 字节，表示采样率，每秒钟的样本数。
     * ByteRate: 4 字节，表示数据传输速率，计算方法为 SampleRate * NumChannels * BitsPerSample / 8。
     * BlockAlign: 2 字节，表示每个采样点的字节数，计算方法为 NumChannels * BitsPerSample / 8。
     * BitsPerSample: 2 字节，表示每个样本的比特数。
     * Subchunk2ID: 4 字节，表示子块2标识符，一般为 "data"。
     * Subchunk2Size: 4 字节，表示音频数据的大小（不包括前面的文件头大小）。
     * */
    struct WAVFileHeader {
        char ChunkID[4] = {'R', 'I', 'F', 'F'};  // 文件标识符，"RIFF"
        uint32_t ChunkSize = 0;// 文件大小，不包括 ChunkID 和 ChunkSize 字段本身, 录制完毕后修改
        char Format[4] = {'W', 'A', 'V', 'E'};   // 文件格式，"WAVE"
        char Subchunk1ID[4] = {'f', 'm', 't', ' '};  // 格式块标识符，"fmt "
        uint32_t Subchunk1Size = 16;             // 格式块大小，固定为 16
        uint16_t AudioFormat = 1;                // 音频格式，PCM 格式为 1
        uint16_t NumChannels;                    // 声道数
        uint32_t SampleRate;                     // 采样率
        uint32_t ByteRate;                       // 每秒的数据量
        uint16_t BlockAlign;                     // 数据块对齐
        uint16_t BitsPerSample;                  // 位深
        char Subchunk2ID[4] = {'d', 'a', 't', 'a'};  // 数据块标识符，"data"
        uint32_t Subchunk2Size = 0;              // 音频数据的大小(初始大小为0, 录制完毕后修改)
    };
};

#endif // AUDIOPLAYER_H
