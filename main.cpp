#include <QApplication>
#include <QSurfaceFormat>
#include <QStringList>
#include "ui/MainWindow.h"
#include "util/Utils.h"
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QIcon>

int main(int argc,char**argv){
    forceCLocaleForMpv();
    QSurfaceFormat fmt;
    fmt.setRenderableType(QSurfaceFormat::OpenGL);
    fmt.setProfile(QSurfaceFormat::CompatibilityProfile);
    fmt.setVersion(3,3);
    QSurfaceFormat::setDefaultFormat(fmt);
    QApplication app(argc,argv);
    QCoreApplication::setApplicationName(QStringLiteral("mpvjoc"));
    QCoreApplication::setOrganizationName(QStringLiteral("mpvjoc"));
    QGuiApplication::setDesktopFileName(QStringLiteral("mpvjoc"));

    QIcon mpvjocIcon;
    const QStringList mpvjocIconCandidates = {
        QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("../assets/mpvjoc.svg")),
        QDir::current().filePath(QStringLiteral("assets/mpvjoc.svg")),
        QDir::home().filePath(QStringLiteral(".local/share/icons/hicolor/scalable/apps/mpvjoc.svg")),
        QStringLiteral("/usr/share/icons/hicolor/scalable/apps/mpvjoc.svg")
    };
    for (const QString& iconPath : mpvjocIconCandidates) {
        if (QFileInfo::exists(iconPath)) {
            mpvjocIcon = QIcon(iconPath);
            break;
        }
    }
    if (!mpvjocIcon.isNull())
        QApplication::setWindowIcon(mpvjocIcon);

    forceCLocaleForMpv();
    MainWindow w;
    w.show();
    QStringList files;
    for(int i=1;i<argc;++i)files<<QString::fromLocal8Bit(argv[i]);
    w.addFiles(files);
    return app.exec();
}
