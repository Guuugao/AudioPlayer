#include "dialog.h"
#include "ui_dialog.h"


Dialog::Dialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::Dialog){

    ui->setupUi(this);
    timer.setTimerType(Qt::PreciseTimer); // 设置毫秒级别精度

    configUI();
    configSignalAndSlot();
}

// TODO 关闭事件需要先停止录制/播放
void Dialog::configSignalAndSlot(){
    connect(ui->recordBtn, &QPushButton::clicked, &this->audioplayer, [this](){
        if (this->audioplayer.isRecording){ // 当前有任务, 必须等待任务完成
            ui->logBrowser->append("is recording");
        } else if (this->audioplayer.isPlaying){
            ui->logBrowser->append("is playing");
        } else { // 无任务
            this->timer.start(refreshInterval); // 1s更新一次计时显示
            ui->logBrowser->append("start record");

            this->audioplayer.startRecord(this->ui->channelBox->currentData().toInt(),
                                          this->ui->bitDepthEdit->text().toInt(),
                                          this->ui->sampleRateEdit->text().toInt(),
                                          this->ui->waveInDeviceBox->currentData().toInt());
        }
    });

    connect(ui->pauseBtn, &QPushButton::clicked, &this->audioplayer, [this](){
        if (this->audioplayer.isRecording) { // 正在录音
            if (!this->audioplayer.isPausing) {
                this->audioplayer.pauseRecord();
                ui->logBrowser->append("pause record");

                this->timer.stop();
                ui->pauseBtn->setText("continue");
            } else {
                this->audioplayer.continueRecord();
                ui->logBrowser->append("continue record");

                this->timer.start();
                ui->pauseBtn->setText("pause");
            }
        } else if (this->audioplayer.isPlaying){ // 正在播放
            if (!this->audioplayer.isPausing) {
                this->audioplayer.pausePlay();
                ui->logBrowser->append("pause play");

                this->timer.stop();
                ui->pauseBtn->setText("continue");
            } else {
                this->audioplayer.continuePlay();
                ui->logBrowser->append("continue play");

                this->timer.start();
                ui->pauseBtn->setText("pause");
            }
        } else { // 没有任务
            ui->logBrowser->append("is not playing or recording");
        }
    });

    connect(ui->stopBtn, &QPushButton::clicked, &this->audioplayer, [this](){
        if (this->audioplayer.isRecording) { // 正在录音
            this->audioplayer.stopRecord();
            this->timer.stop();
            QString fileName = "";
            ui->logBrowser->append("stop record");
            if(QMessageBox::Save == QMessageBox::question(this, "question",
                                                           "Do you want to save the recorded audio?",
                                                           QMessageBox::Save | QMessageBox::Cancel,
                                                           QMessageBox::Save)){
                fileName = QFileDialog::getSaveFileName(this, "Save Audio File", "", "WAV Files (*.wav)");
                if (!fileName.isEmpty()) {
                    this->audioplayer.saveWaveFile(fileName);
                    ui->logBrowser->append("save " + fileName);
                } else {
                    ui->logBrowser->append("cancle");
                }
            }
            this->audioplayer.clearData();
            this->ui->timeLCD->display("00:00:00");
        } else if (this->audioplayer.isPlaying){ // 正在播放
            this->timer.stop();
            this->audioplayer.stopPlay();

            ui->logBrowser->append("stop play");
            this->ui->timeLCD->display("00:00:00");
        } else { // 没有任务
            ui->logBrowser->append("is not playing or recording");
        }
    });

    // TODO 播放时显示比特率, 采样率等信息
    connect(ui->playBtn, &QPushButton::clicked, this, [this](){
        if (this->audioplayer.isRecording){
            ui->logBrowser->append("is recording");
        } else if (this->audioplayer.isPlaying){
            ui->logBrowser->append("is playing");
        } else {
            QString fileName = QFileDialog::getOpenFileName(this, "Open File", "", "WAV Files (*.wav)");
            if (!fileName.isEmpty()) {
                this->timer.start(refreshInterval); // 1s更新一次计时显示
                ui->logBrowser->append("start play");

                this->audioplayer.startPlay(fileName,
                                            ui->waveOutDeviceBox->currentData().toInt());
            } else {
                ui->logBrowser->append("cancle open file");
            }
        }
    });

    connect(&this->timer, &QTimer::timeout, ui->timeLCD, [=](){
        if (audioplayer.isRecording) {
            // 已录制时长
            int recorded = this->audioplayer.startTime.msecsTo(QTime::currentTime());
            QTime show = QTime(0, 0, 0, 0) .addMSecs(recorded);
            ui->timeLCD->display(show.toString("hh:mm:ss"));
        } else if (audioplayer.isPlaying) {
            // 已播放时长, 当前时间与开始播放时间之差
            int played = this->audioplayer.startTime.msecsTo(QTime::currentTime());
            int remaining = this->audioplayer.audioDuration - played;

            // 四舍五入到最接近的整秒
            int seconds = qRound(static_cast<double>(remaining) / 1000.0);

            if (remaining <= 0) {
                this->timer.stop();
                audioplayer.stopPlay();
                this->ui->timeLCD->display("00:00:00");
                ui->logBrowser->append("stop play");
            } else {
                QTime show = QTime(0, 0, 0, 0) .addSecs(seconds);
                ui->timeLCD->display(show.toString("hh:mm:ss"));
            }
        }
    });
}

void Dialog::configUI(){
    int iAudioDev = waveInGetNumDevs();    //获取输入设备数量
    for (int i = 0; i < iAudioDev; i++){
        WAVEINCAPS wic;
        waveInGetDevCaps(i, &wic, sizeof(WAVEINCAPS));   //注意，i即为DeviceID
        ui->waveInDeviceBox->addItem(QString::fromWCharArray(wic.szPname), i);
    }

    int oAudioDev = waveOutGetNumDevs();    //获取输出设备数量
    for (int i = 0; i < oAudioDev; i++){
        WAVEOUTCAPS woc;
        waveOutGetDevCaps(i, &woc, sizeof(WAVEOUTCAPS));   //注意，i即为DeviceID
        ui->waveOutDeviceBox->addItem(QString::fromWCharArray(woc.szPname), i);
    }

    ui->channelBox->addItem("单声道", 1);
    ui->channelBox->addItem("双声道", 2);

    ui->timeLCD->display("00:00:00");

    // 添加验证器, 只允许输入整数
    ui->bitDepthEdit->setValidator(new QIntValidator(ui->bitDepthEdit));
    ui->sampleRateEdit->setValidator(new QIntValidator(ui->bitDepthEdit));
}

void Dialog::closeEvent(QCloseEvent *event){
    if (this->audioplayer.isPlaying) {
        this->audioplayer.stopPlay();
    } else if (this->audioplayer.isRecording) {
        this->audioplayer.stopRecord();
    }
}

Dialog::~Dialog(){
    delete ui;
}

