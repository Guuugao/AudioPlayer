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

void Dialog::configSignalAndSlot(){
    connect(ui->recordBtn, &QPushButton::clicked, &this->audioplayer, [this](){
        if (this->audioplayer.isRecording){ // 当前有任务, 必须等待任务完成
            ui->logBrowser->append("is recording");
        } else if (this->audioplayer.isPlaying){
            ui->logBrowser->append("is playing");
        } else { // 无任务
            this->timer.start(10); // 10ms更新一次计时显示

            ui->logBrowser->append("start record");
            this->audioplayer.startRecord(this->ui->channelBox->currentData().toInt(),
                                          this->ui->bitDepthEdit->text().toInt(),
                                          this->ui->sampleRateEdit->text().toInt(),
                                          this->ui->waveInDeviceBox->currentData().toInt());
        }
    });
    // TODO 暂停和结束按钮通用, 需要判断当前在录音还是播放
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
            this->timer.stop();
            this->audioplayer.stopRecord();

            ui->logBrowser->append("stop record");
            if(QMessageBox::Save == QMessageBox::question(this, "question",
                                                           "Do you want to save the recorded audio?",
                                                           QMessageBox::Save | QMessageBox::Cancel,
                                                           QMessageBox::Save)){
                QString fileName = QFileDialog::getSaveFileName(this, "Save Audio File", "", "WAV Files (*.wav)");
                if (!fileName.isEmpty()) {
                    this->audioplayer.saveWaveFile(fileName);
                    ui->logBrowser->append("save file: " + fileName);
                } else {
                    ui->logBrowser->append("cancle save file");
                }
            }
            this->audioplayer.clearBuffer();
            this->ui->timeLCD->display("00:00:000");
        } else if (this->audioplayer.isPlaying){ // 正在播放
            // 停止播放, 关闭设备...
        } else { // 没有任务
            ui->logBrowser->append("is not playing or recording");
        }
    });

    // TODO 播放时选择播放文件
    connect(ui->playBtn, &QPushButton::clicked, this, [this](){
        if (this->audioplayer.isRecording || this->audioplayer.isPlaying){
            return;
        }

        qDebug() << "play clicked...";
    });

    // TODO 播放和录制的计时逻辑稍有不同
    connect(&this->timer, &QTimer::timeout, ui->timeLCD, [=](){
        QTime diff = QTime(0, 0, 0, 0)
                         .addMSecs(this->audioplayer.startTime.msecsTo(QTime::currentTime()));
        ui->timeLCD->display(diff.toString("mm:ss:zzz"));
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

    ui->timeLCD->display("00:00:000");

    // 添加验证器, 只允许输入整数
    ui->bitDepthEdit->setValidator(new QIntValidator(ui->bitDepthEdit));
    ui->sampleRateEdit->setValidator(new QIntValidator(ui->bitDepthEdit));
}

Dialog::~Dialog(){
    delete ui;
}

