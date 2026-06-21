#include <QApplication>
#include <QSurfaceFormat>
#include <QStringList>
#include "ui/MainWindow.h"
#include "util/Utils.h"

int main(int argc,char**argv){
    forceCLocaleForMpv();
    QSurfaceFormat fmt;
    fmt.setRenderableType(QSurfaceFormat::OpenGL);
    fmt.setProfile(QSurfaceFormat::CompatibilityProfile);
    fmt.setVersion(3,3);
    QSurfaceFormat::setDefaultFormat(fmt);
    QApplication app(argc,argv);
    forceCLocaleForMpv();
    MainWindow w;
    w.show();
    QStringList files;
    for(int i=1;i<argc;++i)files<<QString::fromLocal8Bit(argv[i]);
    w.addFiles(files);
    return app.exec();
}
