#include <QApplication>
#include <QFileDialog>
#include <ffmpegengine.h>

extern "C" {
#include <libavformat/avformat.h>
#include <libavfilter/avfilter.h>
}

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    QFileDialog fd(nullptr, "Import media...", "", "All Files  (*) ");
    fd.setFileMode(QFileDialog::ExistingFiles);
    if (fd.exec()) {
        QStringList fls = fd.selectedFiles();
        FFmpegEngine * fe = new FFmpegEngine(fls);
        fe->run();
    }
    return a.exec();
}
