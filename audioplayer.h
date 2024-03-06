#ifndef AUDIOPLAYER_H
#define AUDIOPLAYER_H

#include <fstream>
#include <windows.h>
#include <thread>
#include <mutex>
#include <condition_variable>

#include <QDebug>
#include <QObject>
#include <QDir>

class AudioPlayer : public QObject
{
    Q_OBJECT
signals:
public:
    // TODO 时间计算采用秒而非毫秒
    QTime startTime; // 开始录制/播放的时间
    QTime pauseTime; // 暂停操作开始的时间
    uint32_t audioDuration; // 音频文件时长(ms)

    bool isRecording = false; // 是否正在录制
    bool isPlaying = false; // 是否正在播放
    bool isPausing = false; // 是否暂停

    // TODO 录制是否会卡顿?
    void startRecord(UINT nChannel, UINT bitDepth, UINT sampleRate, UINT deviceID); // 开始录制
    void pauseRecord(); // 暂停录制
    void continueRecord(); // 继续录制
    void stopRecord(); // 结束录制, 传入文件名不为空则保存文件
    // 保存wave文件
    void saveWaveFile(QString &fileName); // 保存文件
    void clearData(); // 清除recordBuffer数据

    void startPlay(QString& fileName, UINT deviceID); // 开始播放
    void pausePlay(); // 暂停播放
    void continuePlay(); // 继续播放
    void stopPlay(); // 结束播放

    explicit AudioPlayer(QObject *parent = nullptr);
    ~AudioPlayer();
private:
    // TODO 这里的写法能不能优化下
    static constexpr int RECORD_WAVEHDR_NUM = 4;
    static constexpr int RECORD_BUFFER_SIZE = 1024 * 1024 * 2; // 2MB
    // 基础的录制/播放秒数, 比特率乘以这个值即为WAVEHDR缓冲区的大小
    static constexpr int BASE_SECOND = 4;
    // 播放和录制WAVEHDR缓冲区的大小, 根据音频信息确定
    // 暂定为1s的数据量
    int recordBlockSize;
    int playBlockSize;

    HWAVEIN hWaveIn;
    HWAVEOUT hWaveOut;

    // 用于多线程填充数据块
    std::mutex fillMutex;
    std::condition_variable fillCV;
    std::thread fillThread;

    std::ofstream ofs;
    std::ifstream ifs;
    std::vector<BYTE> recordBuffer; // 录制音频缓冲区
    std::vector<PWAVEHDR> recordWaveHeaders; // 录制缓冲区数组
    PWAVEHDR currPlayWaveHeader; // 当前正在使用的缓冲区下标
    PWAVEHDR playWaveHeader_1; // 一号播放缓冲区
    PWAVEHDR playWaveHeader_2; // 二号播放缓冲区

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
    // 读取wave文件头信息, 返回文件格式信息, 填充文件时长数据(ms)
    WAVEFORMATEX readWaveHeader();
    // 播放当前数据块时, 异步加载另一个数据块
    // 需要在nextWaveHeader之后调用
    // TODO 还是会轻微卡顿, 尝试多缓冲区或者检查一下异步填充速度与播放速度是否匹配
    // (多缓冲区可以根据数据块状态进行填充, 一旦检测到一个缓冲区用完, 则填充他, 否则阻塞等待)
    void fillNextWaveHeader();
    // 将当前块指针指向下一块未播放的数据块, 需要传递上一个播放完毕的块指针
    PWAVEHDR nextWaveHeader();

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
