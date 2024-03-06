#ifndef DIALOG_H
#define DIALOG_H

#include <QDialog>
#include <QTimer>
#include <QMessageBox>
#include <QFileDialog>
#include <QDesktopServices>

#include "audioplayer.h"

QT_BEGIN_NAMESPACE
namespace Ui { class Dialog; }
QT_END_NAMESPACE

class Dialog : public QDialog
{
    Q_OBJECT

public:
    Dialog(QWidget *parent = nullptr);
    ~Dialog();

private:
    Ui::Dialog *ui;
    AudioPlayer audioplayer;
    QTimer timer;
    // 刷新计时器的时间间隔
    constexpr static int refreshInterval = 1000;

    // 连接信号与槽
    void configSignalAndSlot();
    // 添加与设定控件
    void configUI();
    // 关闭窗口时释放资源
    void closeEvent(QCloseEvent *event) override;
};
#endif // DIALOG_H
