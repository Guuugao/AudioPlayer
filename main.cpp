#include "dialog.h"
#include <QApplication>

#pragma comment(lib, "winmm.lib")

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    Dialog w;
    w.show();
    return a.exec();
}
