#include "playlist/PlaylistDelegate.h"
#include "MainWindow.h"
#include <QColor>
#include "TimelineWidget.h"
#include "mpv/MpvWidget.h"
#include "playlist/PlaylistModel.h"
#include "util/Utils.h"
#include <QFont>
#include <QLabel>
#include <QListView>
#include <QPushButton>
#include <QTimer>
#include <QtGlobal>
#include <QLineEdit>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QFileDialog>
#include <QFrame>
#include <QHBoxLayout>
#include <QSettings>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QWidget>
#include <QGroupBox>
#include <QCheckBox>
#include <QComboBox>
#include <QSignalBlocker>
#include <QSet>
#include <QList>
#include <QPair>
#include "Version.h"
#include <QEventLoop>
#include <QShortcut>
#include <QKeySequence>
#include <QScrollArea>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QPen>

namespace {
class ResizableSettingsPanel : public QFrame {
public:
    explicit ResizableSettingsPanel(QWidget* parent = nullptr)
        : QFrame(parent)
    {
        setMouseTracking(true);
    }

protected:
    void mousePressEvent(QMouseEvent* event) override
    {
        if (event->button() == Qt::LeftButton && gripRect().contains(event->pos())) {
            resizing = true;
            resizeStartGlobal = event->globalPosition().toPoint();
            resizeStartSize = size();
            event->accept();
            return;
        }

        if (event->button() == Qt::LeftButton && dragRect().contains(event->pos())) {
            moving = true;
            moveStartGlobal = event->globalPosition().toPoint();
            moveStartPos = pos();
            setCursor(Qt::SizeAllCursor);
            event->accept();
            return;
        }

        QFrame::mousePressEvent(event);
    }

    void mouseMoveEvent(QMouseEvent* event) override
    {
        if (resizing) {
            const QPoint delta = event->globalPosition().toPoint() - resizeStartGlobal;
            QSize requested(resizeStartSize.width() + delta.x(), resizeStartSize.height() + delta.y());
            requested = requested.expandedTo(minimumSize());
            if (parentWidget())
                requested = requested.boundedTo(parentWidget()->size() - QSize(24, 24));
            resize(requested);
            event->accept();
            return;
        }

        if (moving) {
            const QPoint delta = event->globalPosition().toPoint() - moveStartGlobal;
            move(boundedPanelPosition(moveStartPos + delta));
            event->accept();
            return;
        }

        if (gripRect().contains(event->pos()))
            setCursor(Qt::SizeFDiagCursor);
        else if (dragRect().contains(event->pos()))
            setCursor(Qt::SizeAllCursor);
        else
            setCursor(Qt::ArrowCursor);

        QFrame::mouseMoveEvent(event);
    }

    void mouseReleaseEvent(QMouseEvent* event) override
    {
        if ((resizing || moving) && event->button() == Qt::LeftButton) {
            resizing = false;
            moving = false;
            if (gripRect().contains(event->pos()))
                setCursor(Qt::SizeFDiagCursor);
            else if (dragRect().contains(event->pos()))
                setCursor(Qt::SizeAllCursor);
            else
                unsetCursor();
            event->accept();
            return;
        }
        QFrame::mouseReleaseEvent(event);
    }

    void leaveEvent(QEvent* event) override
    {
        if (!resizing && !moving)
            unsetCursor();
        QFrame::leaveEvent(event);
    }

    void paintEvent(QPaintEvent* event) override
    {
        QFrame::paintEvent(event);
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);

        // Subtle title drag hint line.
        painter.setPen(QPen(QColor(90, 90, 90), 1));
        painter.drawLine(12, 42, width() - 12, 42);

        // Lower-right resize grip.
        painter.setPen(QPen(QColor(150, 150, 150), 1));
        const int right = width() - 10;
        const int bottom = height() - 10;
        painter.drawLine(right - 18, bottom, right, bottom - 18);
        painter.drawLine(right - 12, bottom, right, bottom - 12);
        painter.drawLine(right - 6, bottom, right, bottom - 6);
    }

private:
    QRect gripRect() const
    {
        return QRect(width() - 28, height() - 28, 28, 28);
    }

    QRect dragRect() const
    {
        return QRect(0, 0, width(), 44);
    }

    QPoint boundedPanelPosition(const QPoint& requested) const
    {
        if (!parentWidget())
            return requested;

        const QSize parentSize = parentWidget()->size();
        const int margin = 12;
        const int minX = margin - width();
        const int minY = margin - height();
        const int maxX = parentSize.width() - margin;
        const int maxY = parentSize.height() - margin;

        // Keep at least a small part of the panel visible so it cannot be lost.
        return QPoint(qBound(minX, requested.x(), maxX), qBound(minY, requested.y(), maxY));
    }

    bool resizing = false;
    bool moving = false;
    QPoint resizeStartGlobal;
    QSize resizeStartSize;
    QPoint moveStartGlobal;
    QPoint moveStartPos;
};
}
void MainWindow::setPlaylistKeyboardFocus(bool focus){
    playlistKeyboardFocus=focus;
    if(focus){if(playlistView)playlistView->setFocus(Qt::TabFocusReason);}else{if(mpvWidget)mpvWidget->setFocus(Qt::TabFocusReason);} if(timeline)timeline->setPlaylistFocusMode(playlistKeyboardFocus);

    applyTimelineHueTheme();
}
void MainWindow::toggleKeyboardFocusTarget(){setPlaylistKeyboardFocus(!playlistKeyboardFocus);}

void MainWindow::updateMuteVolumeButton(){
    if(!muteButton)return;
    muteButton->setText(currentMuted?QStringLiteral("Muted"):QStringLiteral("Volume: %1%").arg(int(currentVolume+0.5)));
    muteButton->setToolTip(currentMuted?QStringLiteral("Muted - click to unmute"):QStringLiteral("Click to mute"));
    muteButton->setStyleSheet(currentMuted
        ? QStringLiteral("QPushButton{background:#6b2020;color:#ffffff;border:1px solid #d05050;padding:2px 7px;text-align:center;}")
        : QStringLiteral("QPushButton{background:#174f25;color:#ffffff;border:1px solid #39a857;padding:2px 7px;text-align:center;}"));
}

void MainWindow::playPauseOrStartSelected(){ if(mpvWidget->currentFilePath().isEmpty()&&playlistModel->count()>0){playPlaylistRow(currentRow());return;} mpvWidget->togglePause();}

void MainWindow::closeCurrentFile(){if(fastPlaybackTimer)fastPlaybackTimer->stop();warpPlaybackMode=false;updateWarpOverlay();suppressNextEndFileAdvance=true;mpvWidget->stopPlayback();position=0;duration=0;timeline->setPosition(0);timeline->setDuration(0);timeline->setTitle(QString());if(titleStatusLabel)titleStatusLabel->setText(QStringLiteral("No file loaded"));updateTimeLabel(0,0);}

double MainWindow::normalizedMaxVideoScale(double scale)const{ if(scale<0.75)return 0.5; if(scale<1.5)return 1.0; return 2.0;}

void MainWindow::updateScaleButtons(){
    if(scaleStatusButton){
        const QString factor=maxVideoScale==0.5?QString::fromUtf8("½x"):(maxVideoScale==2.0?QStringLiteral("2x"):QStringLiteral("1x"));
        QString actual=mpvWidget?mpvWidget->currentScalePercentText():QString();
        if(actual.isEmpty())actual=QStringLiteral("--%");
        scaleStatusButton->setText(QStringLiteral("%1 · %2 actual").arg(factor,actual));
        scaleStatusButton->setToolTip(QStringLiteral("Click to cycle ½x, 1x and 2x"));
        scaleStatusButton->setStyleSheet(QStringLiteral("QPushButton{background:#252525;color:#eeeeee;border:1px solid #666;padding:2px 7px;text-align:center;} QPushButton:hover{background:#333333;}"));
    }
    updateClipButton();
}

void MainWindow::updateClipButton(){
    if(!clipButton)return;
    clipButton->setText(clipVideoToScale?QStringLiteral("clipped"):QStringLiteral("scaled"));
    clipButton->setToolTip(clipVideoToScale?QStringLiteral("Exact scale may clip video - click for scaled mode"):QStringLiteral("Video is scaled down to fit - click for clipped mode"));
    clipButton->setStyleSheet(clipVideoToScale
        ? QStringLiteral("QPushButton{background:#6b2020;color:#ffffff;border:1px solid #d05050;padding:2px 7px;text-align:center;}")
        : QStringLiteral("QPushButton{background:#174f25;color:#ffffff;border:1px solid #39a857;padding:2px 7px;text-align:center;}"));
}

void MainWindow::toggleShortcutHelpOverlay(){
    if(shortcutHelpOverlay&&shortcutHelpOverlay->isVisible()){
        shortcutHelpOverlay->hide();
        return;
    }
    if(!shortcutHelpOverlay){
        shortcutHelpOverlay=new QLabel(this);
        shortcutHelpOverlay->setTextFormat(Qt::PlainText);
        shortcutHelpOverlay->setWordWrap(false);
        shortcutHelpOverlay->setAlignment(Qt::AlignLeft|Qt::AlignTop);
        shortcutHelpOverlay->setAttribute(Qt::WA_StyledBackground,true);
        QFont f=shortcutHelpOverlay->font();
        f.setFamily(QStringLiteral("monospace"));
        f.setPointSize(9);
        shortcutHelpOverlay->setFont(f);
        shortcutHelpOverlay->setStyleSheet(QStringLiteral(
            "QLabel{background:rgba(0,0,0,215);color:#eeeeee;border:2px solid #66ccff;border-radius:8px;padding:14px;}"));
        shortcutHelpOverlay->setText(shortcutHelpText());
    }
    const int w=qMin(width()-40,620);
    const int h=qMax(260,height()-40);
    shortcutHelpOverlay->setGeometry((width()-w)/2,(height()-h)/2,w,h);
    shortcutHelpOverlay->show();
    shortcutHelpOverlay->raise();
}

void MainWindow::setMaxVideoScale(double scale){ maxVideoScale=normalizedMaxVideoScale(scale); if(mpvWidget){mpvWidget->setMaxVideoScale(maxVideoScale);mpvWidget->showScaleOverlay();} updateScaleButtons(); if(!restoringPlaybackState)savePlaylistState();}

void MainWindow::setClipVideoToScale(bool crop){clipVideoToScale=crop;if(mpvWidget){mpvWidget->setClipVideoToScale(clipVideoToScale);mpvWidget->showScaleOverlay();}updateClipButton();if(!restoringPlaybackState)savePlaylistState();}

void MainWindow::setHardwareDecodingMode(const QString&mode){
    QString normalized=mode.trimmed().toLower();
    if(normalized.isEmpty()||normalized==QStringLiteral("software"))normalized=QStringLiteral("no");
    hardwareDecodingMode=normalized;
    if(hardwareDecodingMode!=QStringLiteral("no")){
        lastHardwareDecodingMode=hardwareDecodingMode;
        QSettings().setValue(QStringLiteral("video/lastHardwareDecodingMode"),lastHardwareDecodingMode);
    }
    QSettings().setValue(QStringLiteral("video/hwdecMode"),hardwareDecodingMode);
    QSettings().setValue(QStringLiteral("video/hardwareDecoding"),hardwareDecodingMode!=QStringLiteral("no"));
    if(mpvWidget)mpvWidget->setHardwareDecodingMode(hardwareDecodingMode);
    updateHardwareDecodingButton();
}
void MainWindow::updateHardwareDecodingButton(){
    if(!hwdecButton)return;
    const bool software=hardwareDecodingMode==QStringLiteral("no");
    hwdecButton->setText(software?QStringLiteral("software"):hardwareDecodingMode);
    hwdecButton->setToolTip(software
        ?QStringLiteral("Software video decoding. Click to restore %1. Restart mpvjoc if the current file does not rebuild cleanly.").arg(lastHardwareDecodingMode)
        :QStringLiteral("mpv hwdec=%1. Click for software decoding. Restart mpvjoc if the current file does not rebuild cleanly.").arg(hardwareDecodingMode));
    hwdecButton->setStyleSheet(software
        ?QStringLiteral("QPushButton{background:#6b2020;color:#ffffff;border:1px solid #d05050;padding:2px 7px;text-align:center;}")
        :QStringLiteral("QPushButton{background:#174f25;color:#ffffff;border:1px solid #39a857;padding:2px 7px;text-align:center;}"));
}
void MainWindow::setAutoPlayNextEnabled(bool enabled){autoPlayNextEnabled=enabled;updateAutoPlayButton();if(!restoringPlaybackState)savePlaylistState();}

void MainWindow::updateAutoPlayButton(){
    if(!autoPlayButton)return;
    autoPlayButton->setText(autoPlayNextEnabled?QStringLiteral("autoplay"):QStringLiteral("manual"));
    autoPlayButton->setToolTip(autoPlayNextEnabled?QStringLiteral("Autoplay next playlist item enabled"):QStringLiteral("Autoplay next playlist item disabled"));
    autoPlayButton->setStyleSheet(autoPlayNextEnabled
        ? QStringLiteral("QPushButton{background:#174f25;color:#ffffff;border:1px solid #39a857;padding:2px 7px;text-align:center;}")
        : QStringLiteral("QPushButton{background:#6b2020;color:#ffffff;border:1px solid #d05050;padding:2px 7px;text-align:center;}"));
}



void MainWindow::loadTimelineColorSettings(){
    QSettings s(QStringLiteral("mpvjoc"),QStringLiteral("mpvjoc"));
    timelineGreenDarkPercent=qBound(0,s.value(QStringLiteral("timeline/greenDarkPercent"),80).toInt(),100);
    timelineGreyDarkPercent=qBound(0,s.value(QStringLiteral("timeline/greyDarkPercent"),80).toInt(),100);
    timelineRedDarkPercent=qBound(0,s.value(QStringLiteral("timeline/redDarkPercent"),80).toInt(),100);
}

void MainWindow::saveTimelineColorSettings()const{
    QSettings s(QStringLiteral("mpvjoc"),QStringLiteral("mpvjoc"));
    s.setValue(QStringLiteral("timeline/greenDarkPercent"),timelineGreenDarkPercent);
    s.setValue(QStringLiteral("timeline/greyDarkPercent"),timelineGreyDarkPercent);
    s.setValue(QStringLiteral("timeline/redDarkPercent"),timelineRedDarkPercent);
}

void MainWindow::loadOverlayCellSettings()
{
    QSettings s(QStringLiteral("mpvjoc"), QStringLiteral("mpvjoc"));
    auto readCell = [&](const QString& key, int fallback) { return qBound(1, s.value(key, fallback).toInt(), 9); };
    auto chooseUnique = [](int requested, int fallback, const QSet<int>& used) {
        requested = qBound(1, requested, 9);
        fallback = qBound(1, fallback, 9);
        if (!used.contains(requested)) return requested;
        if (!used.contains(fallback)) return fallback;
        for (int cell = 1; cell <= 9; ++cell) if (!used.contains(cell)) return cell;
        return fallback;
    };
    QSet<int> used;
    overlayCenterCell = chooseUnique(readCell(QStringLiteral("overlay/centerCell"), 5), 5, used);
    used.insert(overlayCenterCell);
    overlayScaleDisplayCell = chooseUnique(readCell(QStringLiteral("overlay/scaleDisplayCell"), 8), 8, used);
    used.insert(overlayScaleDisplayCell);
    overlayVolumeCell = chooseUnique(readCell(QStringLiteral("overlay/volumeCell"), 6), 6, used);
    used.insert(overlayVolumeCell);
    overlayVisibilityMapCell = chooseUnique(readCell(QStringLiteral("overlay/visibilityMapCell"), 9), 9, used);
    used.insert(overlayVisibilityMapCell);
    overlayWarpLabelCell = chooseUnique(readCell(QStringLiteral("overlay/warpLabelCell"), 3), 3, used);

    overlayCenterOpacity = qBound(0, s.value(QStringLiteral("overlay/centerOpacity"), overlayCenterOpacity).toInt(), 100);
    overlayScaleDisplayOpacity = qBound(0, s.value(QStringLiteral("overlay/scaleDisplayOpacity"), overlayScaleDisplayOpacity).toInt(), 100);
    overlayVolumeOpacity = qBound(0, s.value(QStringLiteral("overlay/volumeOpacity"), overlayVolumeOpacity).toInt(), 100);
    overlayVisibilityMapOpacity = qBound(0, s.value(QStringLiteral("overlay/visibilityMapOpacity"), overlayVisibilityMapOpacity).toInt(), 100);
    overlayWarpLabelOpacity = qBound(0, s.value(QStringLiteral("overlay/warpLabelOpacity"), overlayWarpLabelOpacity).toInt(), 100);
}





void MainWindow::saveOverlayCellSettings()
{
    QSettings s(QStringLiteral("mpvjoc"), QStringLiteral("mpvjoc"));
    s.setValue(QStringLiteral("overlay/centerCell"), overlayCenterCell);
    s.setValue(QStringLiteral("overlay/scaleDisplayCell"), overlayScaleDisplayCell);
    s.setValue(QStringLiteral("overlay/volumeCell"), overlayVolumeCell);
    s.setValue(QStringLiteral("overlay/visibilityMapCell"), overlayVisibilityMapCell);
    s.setValue(QStringLiteral("overlay/warpLabelCell"), overlayWarpLabelCell);
    s.setValue(QStringLiteral("overlay/centerOpacity"), overlayCenterOpacity);
    s.setValue(QStringLiteral("overlay/scaleDisplayOpacity"), overlayScaleDisplayOpacity);
    s.setValue(QStringLiteral("overlay/volumeOpacity"), overlayVolumeOpacity);
    s.setValue(QStringLiteral("overlay/visibilityMapOpacity"), overlayVisibilityMapOpacity);
    s.setValue(QStringLiteral("overlay/warpLabelOpacity"), overlayWarpLabelOpacity);
}




void MainWindow::applyOverlayCellSettings()
{
    if (mpvWidget) {
        mpvWidget->setOverlayCells(overlayCenterCell, overlayScaleDisplayCell, overlayVolumeCell, overlayVisibilityMapCell, overlayWarpLabelCell);
        mpvWidget->setOverlayOpacities(overlayCenterOpacity, overlayScaleDisplayOpacity, overlayVolumeOpacity, overlayVisibilityMapOpacity, overlayWarpLabelOpacity);
    }
}

void MainWindow::loadOverlayProfileSettings()
{
    QSettings s(QStringLiteral("mpvjoc"), QStringLiteral("mpvjoc"));
    auto readMask=[&](const QString& key,int fallback){int mask=s.value(key,fallback).toInt()&OverlayAll;return mask?mask:fallback;};
    overlayProfilePersistentInfo=readMask(QStringLiteral("overlayProfiles/persistentInfo"),OverlayAll);
    overlayProfilePlayStateChange=readMask(QStringLiteral("overlayProfiles/playStateChange"),OverlayPlayState|OverlayScaleDisplay);
    overlayProfileVolumeChange=readMask(QStringLiteral("overlayProfiles/volumeChange"),OverlayVolume);
    overlayProfileWarpMode=readMask(QStringLiteral("overlayProfiles/warpMode"),OverlayWarpLabel);
    overlayProfileScaleChange=readMask(QStringLiteral("overlayProfiles/scaleChange"),OverlayScaleDisplay);
}

void MainWindow::saveOverlayProfileSettings()
{
    QSettings s(QStringLiteral("mpvjoc"), QStringLiteral("mpvjoc"));
    s.setValue(QStringLiteral("overlayProfiles/persistentInfo"),overlayProfilePersistentInfo);
    s.setValue(QStringLiteral("overlayProfiles/playStateChange"),overlayProfilePlayStateChange);
    s.setValue(QStringLiteral("overlayProfiles/volumeChange"),overlayProfileVolumeChange);
    s.setValue(QStringLiteral("overlayProfiles/warpMode"),overlayProfileWarpMode);
    s.setValue(QStringLiteral("overlayProfiles/scaleChange"),overlayProfileScaleChange);
}

void MainWindow::applyOverlayProfileSettings()
{
    if(mpvWidget)mpvWidget->setOverlayProfiles(overlayProfilePersistentInfo,overlayProfilePlayStateChange,overlayProfileVolumeChange,overlayProfileWarpMode,overlayProfileScaleChange);
}







QString MainWindow::defaultEverythingRustDatabasePath()const
{
    return QDir::home().filePath(QStringLiteral(".local/share/everything-rust/files.db"));
}

void MainWindow::loadFamilyDestinationSettings()
{
    QSettings settings;
    everythingRustDatabasePath = settings
        .value(QStringLiteral("family/everythingRustDatabasePath"), defaultEverythingRustDatabasePath())
        .toString()
        .trimmed();
    if (everythingRustDatabasePath.isEmpty())
        everythingRustDatabasePath = defaultEverythingRustDatabasePath();
}

void MainWindow::saveFamilyDestinationSettings()
{
    QSettings().setValue(QStringLiteral("family/everythingRustDatabasePath"), everythingRustDatabasePath);
}

bool MainWindow::refreshFamilyDestinations(QString*errorOut)
{
    QVector<FamilyDestination> loaded;
    QString error;
    if (!FamilyDestinationRepository::loadReadOnly(everythingRustDatabasePath, loaded, &error)) {
        familyDestinationsLastError = error;
        if (errorOut)
            *errorOut = error;
        return false; // Preserve the previous in-memory list.
    }

    familyDestinations = std::move(loaded);
    PlaylistDelegate::setFamilyDestinations(familyDestinations);
    if(playlistView&&playlistView->viewport())playlistView->viewport()->update();
    familyDestinationsLastError.clear();
    if (errorOut)
        errorOut->clear();
    return true;
}

void MainWindow::openSettingsDialog()
{
    QWidget* host = window();
    if (!host)
        return;

    QWidget overlay(host);
    overlay.setObjectName(QStringLiteral("settingsOverlay"));
    overlay.setAttribute(Qt::WA_StyledBackground, true);
    overlay.setFocusPolicy(Qt::StrongFocus);
    overlay.setGeometry(host->rect());
    overlay.setStyleSheet(QStringLiteral(
        "#settingsOverlay { background-color: rgba(0, 0, 0, 150); }"));

    ResizableSettingsPanel dialog(&overlay);
    dialog.setObjectName(QStringLiteral("settingsPanel"));
    dialog.setFrameShape(QFrame::StyledPanel);
    const QSize defaultSettingsPanelSize(qMin(1040, qMax(720, host->width() - 80)), qBound(640, host->height() - 80, 900));
    dialog.setMinimumSize(720, 520);

    QSettings settingsPanelSettings;
    QSize rememberedSettingsPanelSize = settingsPanelSettings.value(QStringLiteral("settings/panelSize"), defaultSettingsPanelSize).toSize();
    rememberedSettingsPanelSize = rememberedSettingsPanelSize.expandedTo(dialog.minimumSize());
    rememberedSettingsPanelSize = rememberedSettingsPanelSize.boundedTo(host->size() - QSize(24, 24));
    if (!rememberedSettingsPanelSize.isValid())
        rememberedSettingsPanelSize = defaultSettingsPanelSize;
    dialog.resize(rememberedSettingsPanelSize);

    const bool hasRememberedSettingsPanelPos = settingsPanelSettings.contains(QStringLiteral("settings/panelPos"));
    const QPoint rememberedSettingsPanelPos = settingsPanelSettings.value(QStringLiteral("settings/panelPos")).toPoint();
    dialog.setToolTip(QStringLiteral("Drag the title area to move settings. Drag the lower-right corner to resize settings."));
    dialog.setStyleSheet(QStringLiteral(
        "#settingsPanel {"
        "  background-color: rgb(32, 32, 32);"
        "  border: 1px solid rgb(130, 130, 130);"
        "  border-radius: 8px;"
        "}"
        "#settingsPanel QLabel { color: rgb(235, 235, 235); }"
        "#settingsPanel QGroupBox { color: rgb(235, 235, 235); }"
        "#settingsPanel QPushButton { padding: 4px 12px; }"));

    auto boundedSettingsPanelPos = [&](const QPoint& requested) {
        const int margin = 12;
        const int minX = margin - dialog.width();
        const int minY = margin - dialog.height();
        const int maxX = overlay.width() - margin;
        const int maxY = overlay.height() - margin;
        return QPoint(qBound(minX, requested.x(), maxX), qBound(minY, requested.y(), maxY));
    };

    auto centerSettingsPanel = [&] {
        overlay.setGeometry(host->rect());
        const QPoint centered((overlay.width() - dialog.width()) / 2,
            (overlay.height() - dialog.height()) / 2);
        dialog.move(boundedSettingsPanelPos(hasRememberedSettingsPanelPos ? rememberedSettingsPanelPos : centered));
    };

    auto* rootLayout = new QVBoxLayout(&dialog);
    rootLayout->setContentsMargins(10, 10, 10, 10);
    rootLayout->setSpacing(8);

    auto* settingsTitle = new QLabel(QStringLiteral("mpvjoc Settings"), &dialog);
    QFont settingsTitleFont = settingsTitle->font();
    settingsTitleFont.setBold(true);
    settingsTitleFont.setPointSizeF(settingsTitleFont.pointSizeF() + 2.0);
    settingsTitle->setFont(settingsTitleFont);
    settingsTitle->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    dialog.setToolTip(QStringLiteral("Drag the title area to move settings. Drag the lower-right corner to resize settings."));
    rootLayout->addWidget(settingsTitle);


    auto* settingsScroll = new QScrollArea(&dialog);
    settingsScroll->setWidgetResizable(true);
    settingsScroll->setFrameShape(QFrame::NoFrame);
    settingsScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto* settingsContent = new QWidget(settingsScroll);
    auto* settingsLayout = new QVBoxLayout(settingsContent);
    settingsLayout->setContentsMargins(4, 4, 10, 4);
    settingsLayout->setSpacing(14);
    settingsScroll->setWidget(settingsContent);
    rootLayout->addWidget(settingsScroll, 1);

    auto scaledColor = [](const QColor& base, int percent) {
        percent = qBound(0, percent, 100);
        return QColor(qBound(0, base.red() * percent / 100, 255),
                      qBound(0, base.green() * percent / 100, 255),
                      qBound(0, base.blue() * percent / 100, 255));
    };

    auto setSwatch = [](QLabel* swatch, const QColor& color) {
        if (!swatch)
            return;
        swatch->setStyleSheet(QStringLiteral("background-color: rgb(%1,%2,%3); border: 1px solid #777;")
                                  .arg(color.red())
                                  .arg(color.green())
                                  .arg(color.blue()));
    };

    auto makeSwatch = [&](const QColor& color, QWidget* parent) {
        auto* swatch = new QLabel(parent);
        swatch->setFixedSize(46, 22);
        swatch->setFrameShape(QFrame::Box);
        setSwatch(swatch, color);
        return swatch;
    };

    auto* timelineGroup = new QGroupBox(QStringLiteral("Timeline"), &dialog);
    auto* timelineLayout = new QFormLayout(timelineGroup);
    timelineLayout->setContentsMargins(10, 8, 10, 10);
    timelineLayout->setSpacing(10);

    auto makeTimelineRow = [&](const QString& title, const QColor& baseColor, int currentPercent) {
        auto* row = new QWidget(timelineGroup);
        row->setMinimumHeight(30);
        auto* rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        rowLayout->setSpacing(8);
        auto* percent = new QSpinBox(row);
        percent->setRange(0, 100);
        percent->setSuffix(QStringLiteral("%"));
        percent->setValue(qBound(0, currentPercent, 100));
        percent->setFixedWidth(78);
        auto* baseLabel = new QLabel(QStringLiteral("base"), row);
        auto* baseSwatch = makeSwatch(baseColor, row);
        auto* darkLabel = new QLabel(QStringLiteral("dark"), row);
        auto* darkSwatch = makeSwatch(scaledColor(baseColor, percent->value()), row);
        rowLayout->addWidget(percent);
        rowLayout->addSpacing(8);
        rowLayout->addWidget(baseLabel);
        rowLayout->addWidget(baseSwatch);
        rowLayout->addWidget(darkLabel);
        rowLayout->addWidget(darkSwatch);
        rowLayout->addStretch(1);
        QObject::connect(percent, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), &dialog,
            [=](int value) { setSwatch(darkSwatch, scaledColor(baseColor, value)); });
        timelineLayout->addRow(title, row);
        return percent;
    };

    auto* greenSpin = makeTimelineRow(QStringLiteral("Green/default dark tone"), QColor(0, 255, 0), timelineGreenDarkPercent);
    auto* greySpin = makeTimelineRow(QStringLiteral("Grey/playlist focus dark tone"), QColor(128, 128, 128), timelineGreyDarkPercent);
    auto* redSpin = makeTimelineRow(QStringLiteral("Red/Warp dark tone"), QColor(255, 0, 0), timelineRedDarkPercent);
    settingsLayout->addWidget(timelineGroup);

    auto* overlayGroup = new QGroupBox(QStringLiteral("Overlay layout"), &dialog);
    auto* overlayLayout = new QFormLayout(overlayGroup);
    overlayLayout->setContentsMargins(10, 8, 10, 10);
    overlayLayout->setSpacing(11);
    auto* gridHint = new QLabel(QStringLiteral("Grid cells: 1 2 3 / 4 5 6 / 7 8 9"), overlayGroup);
    gridHint->setWordWrap(true);
    overlayLayout->addRow(QStringLiteral("Grid"), gridHint);

    auto makeCellCombo = [&](int currentValue) {
        auto* combo = new QComboBox(overlayGroup);
        combo->setFixedWidth(76);
        combo->setEditable(false);
        combo->setSizeAdjustPolicy(QComboBox::AdjustToContents);
        combo->setProperty("cell", qBound(1, currentValue, 9));
        return combo;
    };
    auto comboCell = [](QComboBox* combo) {
        return combo->currentData().isValid() ? combo->currentData().toInt() : combo->property("cell").toInt();
    };
    auto setComboChoices = [&](QComboBox* combo, int currentValue, const QSet<int>& blocked) {
        currentValue = qBound(1, currentValue, 9);
        QSignalBlocker blocker(combo);
        combo->clear();
        for (int cell = 1; cell <= 9; ++cell) {
            if (cell == currentValue || !blocked.contains(cell))
                combo->addItem(QString::number(cell), cell);
        }
        const int index = combo->findData(currentValue);
        if (index >= 0)
            combo->setCurrentIndex(index);
        combo->setProperty("cell", currentValue);
    };

    auto* playStateCellCombo = makeCellCombo(overlayCenterCell);
    auto* scaleDisplayCellCombo = makeCellCombo(overlayScaleDisplayCell);
    auto* volumeCellCombo = makeCellCombo(overlayVolumeCell);
    auto* mapCellCombo = makeCellCombo(overlayVisibilityMapCell);
    auto* warpCellCombo = makeCellCombo(overlayWarpLabelCell);

    auto updateOverlayCombos = [&]() {
        int playState = qBound(1, comboCell(playStateCellCombo), 9);
        int scaleDisplay = qBound(1, comboCell(scaleDisplayCellCombo), 9);
        int volume = qBound(1, comboCell(volumeCellCombo), 9);
        int map = qBound(1, comboCell(mapCellCombo), 9);
        int warp = qBound(1, comboCell(warpCellCombo), 9);
        QSet<int> used;
        auto unique = [&](int requested, int fallback) {
            requested = qBound(1, requested, 9);
            fallback = qBound(1, fallback, 9);
            if (!used.contains(requested)) { used.insert(requested); return requested; }
            if (!used.contains(fallback)) { used.insert(fallback); return fallback; }
            for (int cell = 1; cell <= 9; ++cell) if (!used.contains(cell)) { used.insert(cell); return cell; }
            return fallback;
        };
        playState = unique(playState, 5);
        scaleDisplay = unique(scaleDisplay, 8);
        volume = unique(volume, 6);
        map = unique(map, 9);
        warp = unique(warp, 3);
        setComboChoices(playStateCellCombo, playState, QSet<int> { scaleDisplay, volume, map, warp });
        setComboChoices(scaleDisplayCellCombo, scaleDisplay, QSet<int> { playState, volume, map, warp });
        setComboChoices(volumeCellCombo, volume, QSet<int> { playState, scaleDisplay, map, warp });
        setComboChoices(mapCellCombo, map, QSet<int> { playState, scaleDisplay, volume, warp });
        setComboChoices(warpCellCombo, warp, QSet<int> { playState, scaleDisplay, volume, map });
    };

    for (auto* combo : { playStateCellCombo, scaleDisplayCellCombo, volumeCellCombo, mapCellCombo, warpCellCombo }) {
        QObject::connect(combo, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), &dialog,
            [&, combo](int) { combo->setProperty("cell", comboCell(combo)); updateOverlayCombos(); });
    }
    updateOverlayCombos();

    auto makeOpacitySpin = [&](int currentValue) {
        auto* spin = new QSpinBox(overlayGroup);
        spin->setRange(0, 100);
        spin->setSuffix(QStringLiteral("%"));
        spin->setValue(qBound(0, currentValue, 100));
        spin->setFixedWidth(78);
        return spin;
    };
    auto makeOverlayRow = [&](QComboBox* cellCombo, QSpinBox* opacitySpin) {
        auto* row = new QWidget(overlayGroup);
        row->setMinimumHeight(30);
        auto* rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        rowLayout->setSpacing(12);
        rowLayout->addWidget(new QLabel(QStringLiteral("cell"), row));
        rowLayout->addWidget(cellCombo);
        rowLayout->addSpacing(12);
        rowLayout->addWidget(new QLabel(QStringLiteral("transparency"), row));
        rowLayout->addWidget(opacitySpin);
        rowLayout->addStretch(1);
        return row;
    };

    auto* playStateOpacitySpin = makeOpacitySpin(overlayCenterOpacity);
    auto* scaleDisplayOpacitySpin = makeOpacitySpin(overlayScaleDisplayOpacity);
    auto* volumeOpacitySpin = makeOpacitySpin(overlayVolumeOpacity);
    auto* mapOpacitySpin = makeOpacitySpin(overlayVisibilityMapOpacity);
    auto* warpOpacitySpin = makeOpacitySpin(overlayWarpLabelOpacity);
    overlayLayout->addRow(QStringLiteral("Play state"), makeOverlayRow(playStateCellCombo, playStateOpacitySpin));
    overlayLayout->addRow(QStringLiteral("Scale display"), makeOverlayRow(scaleDisplayCellCombo, scaleDisplayOpacitySpin));
    overlayLayout->addRow(QStringLiteral("Volume overlay"), makeOverlayRow(volumeCellCombo, volumeOpacitySpin));
    overlayLayout->addRow(QStringLiteral("Visibility map"), makeOverlayRow(mapCellCombo, mapOpacitySpin));
    overlayLayout->addRow(QStringLiteral("Warp label"), makeOverlayRow(warpCellCombo, warpOpacitySpin));
    settingsLayout->addWidget(overlayGroup);

    auto* profilesGroup = new QGroupBox(QStringLiteral("Overlay profiles"), &dialog);
    auto* profilesLayout = new QVBoxLayout(profilesGroup);
    profilesLayout->setContentsMargins(10, 8, 10, 10);
    profilesLayout->setSpacing(11);
    auto* profilesHint = new QLabel(QStringLiteral("Choose which overlay elements are shown for each overlay mode."), profilesGroup);
    profilesHint->setWordWrap(true);
    profilesLayout->addWidget(profilesHint);

    auto makeProfileRow = [&](const QString& title, int mask) {
        auto* row = new QWidget(profilesGroup);
        row->setMinimumHeight(30);
        auto* rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        rowLayout->setSpacing(12);
        auto* titleLabel = new QLabel(title, row);
        titleLabel->setMinimumWidth(160);
        rowLayout->addWidget(titleLabel);
        auto* play = new QCheckBox(QStringLiteral("Play"), row);
        auto* scale = new QCheckBox(QStringLiteral("Scale"), row);
        auto* volume = new QCheckBox(QStringLiteral("Volume"), row);
        auto* map = new QCheckBox(QStringLiteral("Map"), row);
        auto* warp = new QCheckBox(QStringLiteral("Warp"), row);
        play->setChecked(mask & OverlayPlayState);
        scale->setChecked(mask & OverlayScaleDisplay);
        volume->setChecked(mask & OverlayVolume);
        map->setChecked(mask & OverlayVisibilityMap);
        warp->setChecked(mask & OverlayWarpLabel);
        rowLayout->addWidget(play);
        rowLayout->addWidget(scale);
        rowLayout->addWidget(volume);
        rowLayout->addWidget(map);
        rowLayout->addWidget(warp);
        rowLayout->addStretch(1);
        return QPair<QWidget*, QList<QCheckBox*>>(row, QList<QCheckBox*> { play, scale, volume, map, warp });
    };

    auto persistentProfile = makeProfileRow(QStringLiteral("F1 persistent"), overlayProfilePersistentInfo);
    auto playStateProfile = makeProfileRow(QStringLiteral("Play state change"), overlayProfilePlayStateChange);
    auto volumeProfile = makeProfileRow(QStringLiteral("Volume change"), overlayProfileVolumeChange);
    auto warpProfile = makeProfileRow(QStringLiteral("Warp mode"), overlayProfileWarpMode);
    auto scaleProfile = makeProfileRow(QStringLiteral("Scale change"), overlayProfileScaleChange);
    profilesLayout->addWidget(persistentProfile.first);
    profilesLayout->addWidget(playStateProfile.first);
    profilesLayout->addWidget(volumeProfile.first);
    profilesLayout->addWidget(warpProfile.first);
    profilesLayout->addWidget(scaleProfile.first);
    settingsLayout->addWidget(profilesGroup);

    auto profileMask = [](const QList<QCheckBox*>& checks) {
        int mask = 0;
        if (checks.value(0) && checks.value(0)->isChecked()) mask |= OverlayPlayState;
        if (checks.value(1) && checks.value(1)->isChecked()) mask |= OverlayScaleDisplay;
        if (checks.value(2) && checks.value(2)->isChecked()) mask |= OverlayVolume;
        if (checks.value(3) && checks.value(3)->isChecked()) mask |= OverlayVisibilityMap;
        if (checks.value(4) && checks.value(4)->isChecked()) mask |= OverlayWarpLabel;
        return mask;
    };

    auto* playbackGroup = new QGroupBox(QStringLiteral("Playback"), &dialog);
    auto* playbackLayout = new QVBoxLayout(playbackGroup);
    playbackLayout->setContentsMargins(10, 8, 10, 10);
    playbackLayout->setSpacing(9);
    auto* autoPlayCheck = new QCheckBox(QStringLiteral("Auto-play next playlist item"), playbackGroup);
    autoPlayCheck->setChecked(autoPlayNextEnabled);
    autoPlayCheck->setToolTip(QStringLiteral("When enabled, end-of-file advances to the next playlist item."));
    auto* clipCheck = new QCheckBox(QStringLiteral("Clip exact video scale when larger than the video area"), playbackGroup);
    clipCheck->setChecked(clipVideoToScale);
    clipCheck->setToolTip(QStringLiteral("When enabled, exact scale may clip video. When disabled, oversized video is scaled down to fit."));
    auto* hardwareDecodeRow = new QWidget(playbackGroup);
    auto* hardwareDecodeRowLayout = new QHBoxLayout(hardwareDecodeRow);
    hardwareDecodeRowLayout->setContentsMargins(0,0,0,0);
    hardwareDecodeRowLayout->setSpacing(8);
    auto* hardwareDecodeLabel = new QLabel(QStringLiteral("Video decoding"),hardwareDecodeRow);
    auto* hardwareDecodeCombo = new QComboBox(hardwareDecodeRow);
    hardwareDecodeCombo->setEditable(true);
    const QStringList hardwareDecodeModes{
        QStringLiteral("no"),
        QStringLiteral("auto-safe"),
        QStringLiteral("auto-copy-safe"),
        QStringLiteral("vaapi"),
        QStringLiteral("vaapi-copy"),
        QStringLiteral("vulkan"),
        QStringLiteral("vulkan-copy"),
        QStringLiteral("nvdec"),
        QStringLiteral("nvdec-copy")
    };
    for(const QString&mode:hardwareDecodeModes)hardwareDecodeCombo->addItem(mode,mode);
    int hardwareDecodeIndex=hardwareDecodeCombo->findData(hardwareDecodingMode);
    if(hardwareDecodeIndex>=0)hardwareDecodeCombo->setCurrentIndex(hardwareDecodeIndex);
    else hardwareDecodeCombo->setEditText(hardwareDecodingMode);
    hardwareDecodeCombo->setToolTip(QStringLiteral("Exact value passed to mpv's hwdec option. 'no' is software decoding. Unsupported modes fall back or fail according to mpv."));
    hardwareDecodeRowLayout->addWidget(hardwareDecodeLabel);
    hardwareDecodeRowLayout->addWidget(hardwareDecodeCombo,1);
    auto* hardwareDecodeNote = new QLabel(QStringLiteral("Decoder changes are requested immediately; restarting mpvjoc guarantees a fresh decoder/render pipeline. Direct hardware modes may corrupt some Dolby Vision/10-bit files in the embedded OpenGL renderer."), playbackGroup);
    hardwareDecodeNote->setWordWrap(true);
    playbackLayout->addWidget(autoPlayCheck);
    playbackLayout->addWidget(clipCheck);
    playbackLayout->addWidget(hardwareDecodeRow);
    playbackLayout->addWidget(hardwareDecodeNote);
    settingsLayout->addWidget(playbackGroup);

    auto* moveSettingsGroup = new QGroupBox(QStringLiteral("Move shortcut destinations"), &dialog);
    auto* moveSettingsLayout = new QFormLayout(moveSettingsGroup);
    moveSettingsLayout->setContentsMargins(10, 8, 10, 10);
    moveSettingsLayout->setSpacing(9);
    auto* moveSettingsHint = new QLabel(QStringLiteral("Configure the destination used by Shift+1 through Shift+6."), moveSettingsGroup);
    moveSettingsHint->setWordWrap(true);
    moveSettingsLayout->addRow(moveSettingsHint);
    QList<QLineEdit*> moveNameEdits;
    QList<QLineEdit*> movePathEdits;
    ensureMoveLists();
    for(int i=0;i<6;++i){
        auto* row = new QWidget(moveSettingsGroup);
        auto* rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(0,0,0,0);
        rowLayout->setSpacing(8);
        auto* nameEdit = new QLineEdit(moveButtonNames.value(i,QStringLiteral("Move %1").arg(i+1)),row);
        nameEdit->setPlaceholderText(QStringLiteral("Move %1").arg(i+1));
        nameEdit->setMaximumWidth(170);
        auto* pathEdit = new QLineEdit(moveButtonPaths.value(i),row);
        pathEdit->setPlaceholderText(QStringLiteral("Destination folder"));
        auto* browse = new QPushButton(QStringLiteral("Browse..."),row);
        browse->setFocusPolicy(Qt::NoFocus);
        rowLayout->addWidget(nameEdit);
        rowLayout->addWidget(pathEdit,1);
        rowLayout->addWidget(browse);
        QObject::connect(browse,&QPushButton::clicked,&dialog,[this,pathEdit]{
            const QString start=pathEdit->text().isEmpty()?QDir::homePath():pathEdit->text();
            const QString selected=QFileDialog::getExistingDirectory(this,QStringLiteral("Choose move destination"),start);
            if(!selected.isEmpty())pathEdit->setText(QDir::cleanPath(selected));
        });
        moveNameEdits<<nameEdit;
        movePathEdits<<pathEdit;
        moveSettingsLayout->addRow(QStringLiteral("Shift+%1").arg(i+1),row);
    }
    settingsLayout->addWidget(moveSettingsGroup);

    auto* familyGroup = new QGroupBox(QStringLiteral("Family destinations"), &dialog);
    auto* familyLayout = new QFormLayout(familyGroup);
    familyLayout->setContentsMargins(10, 8, 10, 10);
    familyLayout->setSpacing(8);

    auto* familyPathRow = new QWidget(familyGroup);
    auto* familyPathLayout = new QHBoxLayout(familyPathRow);
    familyPathLayout->setContentsMargins(0, 0, 0, 0);
    familyPathLayout->setSpacing(8);
    auto* familyDatabaseEdit = new QLineEdit(everythingRustDatabasePath, familyPathRow);
    auto* familyBrowseButton = new QPushButton(QStringLiteral("Browse..."), familyPathRow);
    familyPathLayout->addWidget(familyDatabaseEdit, 1);
    familyPathLayout->addWidget(familyBrowseButton);
    familyLayout->addRow(QStringLiteral("Everything-rust database"), familyPathRow);

    auto familySummaryText = [](const QVector<FamilyDestination>& destinations, const QString& error) {
        if (!error.isEmpty())
            return QStringLiteral("Error: %1").arg(error);
        int available = 0;
        for (const FamilyDestination& destination : destinations)
            if (destination.available) ++available;
        return QStringLiteral("Loaded: %1  •  available: %2  •  unavailable: %3")
            .arg(destinations.size()).arg(available).arg(destinations.size() - available);
    };

    auto* familyStatusLabel = new QLabel(familySummaryText(familyDestinations, familyDestinationsLastError), familyGroup);
    familyStatusLabel->setWordWrap(true);
    familyStatusLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    auto* familyRefreshButton = new QPushButton(QStringLiteral("Refresh destinations"), familyGroup);
    familyLayout->addRow(QStringLiteral("Status"), familyStatusLabel);
    familyLayout->addRow(QString(), familyRefreshButton);

    QObject::connect(familyBrowseButton, &QPushButton::clicked, &dialog, [&, familyDatabaseEdit] {
        const QString current = familyDatabaseEdit->text().trimmed();
        const QString start = current.isEmpty() ? QDir::homePath() : QFileInfo(current).absolutePath();
        const QString chosen = QFileDialog::getOpenFileName(&dialog,
            QStringLiteral("Select everything-rust SQLite database"),
            start,
            QStringLiteral("SQLite databases (*.db *.sqlite *.sqlite3);;All files (*)"));
        if (!chosen.isEmpty())
            familyDatabaseEdit->setText(chosen);
    });

    QObject::connect(familyRefreshButton, &QPushButton::clicked, &dialog, [=] {
        QVector<FamilyDestination> preview;
        QString error;
        const QString candidate = familyDatabaseEdit->text().trimmed();
        if (FamilyDestinationRepository::loadReadOnly(candidate, preview, &error))
            familyStatusLabel->setText(familySummaryText(preview, QString()));
        else
            familyStatusLabel->setText(familySummaryText(preview, error));
    });

    rootLayout->addWidget(familyGroup);

    auto* playlistGroup = new QGroupBox(QStringLiteral("Playlist"), &dialog);
    auto* playlistLayout = new QVBoxLayout(playlistGroup);
    playlistLayout->setContentsMargins(10, 8, 10, 10);
    playlistLayout->setSpacing(9);
    auto* searchHint = new QLabel(QStringLiteral("Search syntax: words are AND terms, prefix with - to exclude, use reviewed/watched/seen for reviewed rows."), playlistGroup);
    searchHint->setWordWrap(true);
    playlistLayout->addWidget(searchHint);
    settingsLayout->addWidget(playlistGroup);
    settingsLayout->addStretch(1);

    
    auto* aboutGroup = new QGroupBox(QStringLiteral("About"), &dialog);
    auto* aboutLayout = new QFormLayout(aboutGroup);
    aboutLayout->setLabelAlignment(Qt::AlignRight);

    auto makeAboutLabel = [&](const QString& text) {
        auto* label = new QLabel(text, aboutGroup);
        label->setTextInteractionFlags(Qt::TextSelectableByMouse);
        label->setWordWrap(true);
        return label;
    };

    aboutLayout->addRow(QStringLiteral("Version"), makeAboutLabel(QString::fromLatin1(MPVJOC_VERSION)));
    aboutLayout->addRow(QStringLiteral("Build"), makeAboutLabel(
        QStringLiteral("%1-%2").arg(QString::fromLatin1(MPVJOC_GIT_COMMIT_COUNT), QString::fromLatin1(MPVJOC_GIT_HASH))));
    aboutLayout->addRow(QStringLiteral("Git"), makeAboutLabel(QString::fromLatin1(MPVJOC_GIT_DESCRIBE)));
    aboutLayout->addRow(QStringLiteral("State"), makeAboutLabel(QString::fromLatin1(MPVJOC_GIT_DIRTY)));
    aboutLayout->addRow(QStringLiteral("Build type"), makeAboutLabel(QString::fromLatin1(MPVJOC_BUILD_TYPE).isEmpty()
            ? QStringLiteral("default")
            : QString::fromLatin1(MPVJOC_BUILD_TYPE)));
    aboutLayout->addRow(QStringLiteral("AI use"), makeAboutLabel(QStringLiteral(
        "This project has been developed with AI-assisted coding support. "
        "All generated or suggested changes are reviewed, tested, and accepted by the project maintainer.")));

    settingsLayout->addWidget(aboutGroup);

auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    bool settingsAccepted = false;
    QEventLoop settingsLoop;
    QObject::connect(buttons, &QDialogButtonBox::accepted, &overlay, [&] { settingsAccepted = true; settingsLoop.quit(); });
    QObject::connect(buttons, &QDialogButtonBox::rejected, &overlay, [&] { settingsAccepted = false; settingsLoop.quit(); });
    rootLayout->addWidget(buttons);

    auto* returnShortcut = new QShortcut(QKeySequence(Qt::Key_Return), &overlay);
    returnShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    QObject::connect(returnShortcut, &QShortcut::activated, &overlay, [&] { settingsAccepted = true; settingsLoop.quit(); });

    auto* enterShortcut = new QShortcut(QKeySequence(Qt::Key_Enter), &overlay);
    enterShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    QObject::connect(enterShortcut, &QShortcut::activated, &overlay, [&] { settingsAccepted = true; settingsLoop.quit(); });

    auto* escapeShortcut = new QShortcut(QKeySequence(Qt::Key_Escape), &overlay);
    escapeShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    QObject::connect(escapeShortcut, &QShortcut::activated, &overlay, [&] { settingsAccepted = false; settingsLoop.quit(); });

    centerSettingsPanel();
    overlay.show();
    overlay.raise();
    dialog.show();
    dialog.raise();
    dialog.setFocus(Qt::OtherFocusReason);
    overlay.grabKeyboard();
    settingsLoop.exec();
    overlay.releaseKeyboard();
    settingsPanelSettings.setValue(QStringLiteral("settings/panelSize"), dialog.size());
    settingsPanelSettings.setValue(QStringLiteral("settings/panelPos"), dialog.pos());
    overlay.hide();

    if (settingsAccepted) {
        timelineGreenDarkPercent = greenSpin->value();
        timelineGreyDarkPercent = greySpin->value();
        timelineRedDarkPercent = redSpin->value();
        overlayCenterCell = comboCell(playStateCellCombo);
        overlayScaleDisplayCell = comboCell(scaleDisplayCellCombo);
        overlayVolumeCell = comboCell(volumeCellCombo);
        overlayVisibilityMapCell = comboCell(mapCellCombo);
        overlayWarpLabelCell = comboCell(warpCellCombo);
        overlayCenterOpacity = playStateOpacitySpin->value();
        overlayScaleDisplayOpacity = scaleDisplayOpacitySpin->value();
        overlayVolumeOpacity = volumeOpacitySpin->value();
        overlayVisibilityMapOpacity = mapOpacitySpin->value();
        overlayWarpLabelOpacity = warpOpacitySpin->value();
        overlayProfilePersistentInfo = profileMask(persistentProfile.second);
        overlayProfilePlayStateChange = profileMask(playStateProfile.second);
        overlayProfileVolumeChange = profileMask(volumeProfile.second);
        overlayProfileWarpMode = profileMask(warpProfile.second);
        overlayProfileScaleChange = profileMask(scaleProfile.second);
        saveTimelineColorSettings();
        saveOverlayCellSettings();
        saveOverlayProfileSettings();
        applyTimelineHueTheme();
        applyOverlayCellSettings();
        applyOverlayProfileSettings();
        everythingRustDatabasePath = familyDatabaseEdit->text().trimmed();
        if (everythingRustDatabasePath.isEmpty())
            everythingRustDatabasePath = defaultEverythingRustDatabasePath();
        saveFamilyDestinationSettings();
        QString familyRefreshError;
        refreshFamilyDestinations(&familyRefreshError);
        setAutoPlayNextEnabled(autoPlayCheck->isChecked());
        setClipVideoToScale(clipCheck->isChecked());
        setHardwareDecodingMode(hardwareDecodeCombo->currentText());
        ensureMoveLists();
        moveButtonCount=6;
        for(int i=0;i<6;++i){
            QString name=moveNameEdits.value(i)->text().trimmed();
            if(name.isEmpty())name=QStringLiteral("Move %1").arg(i+1);
            moveButtonNames[i]=name;
            moveButtonPaths[i]=QDir::cleanPath(movePathEdits.value(i)->text().trimmed());
            if(movePathEdits.value(i)->text().trimmed().isEmpty())moveButtonPaths[i].clear();
        }
        saveMoveConfig();
        updateMoveButtons();
        savePlaylistState();
    }
}









void MainWindow::applyTimelineHueTheme(){
    if(!timeline)return;
    static bool timelineColorSettingsLoaded=false;
    if(!timelineColorSettingsLoaded){loadTimelineColorSettings();loadOverlayCellSettings();loadOverlayProfileSettings();timelineColorSettingsLoaded=true;}
    const QColor defaultNormalHue(0,255,0);
    const QColor defaultFocusHue(255,0,0);
    const QColor playlistGreyHue(128,128,128);
    const QColor warpRedHue(255,0,0);
    const bool playlistFocusNow=playlistKeyboardFocus||(playlistView&&playlistView->hasFocus())||(playlistSearchEdit&&playlistSearchEdit->hasFocus());
    if(warpPlaybackMode){
        timeline->setTimelineDarkTonePercents(timelineRedDarkPercent,timelineRedDarkPercent);timeline->setTimelineHues(warpRedHue,warpRedHue);
    }else if(playlistFocusNow){
        timeline->setTimelineDarkTonePercents(timelineGreyDarkPercent,timelineGreyDarkPercent);timeline->setTimelineHues(playlistGreyHue,playlistGreyHue);
    }else{
        timeline->setTimelineDarkTonePercents(timelineGreenDarkPercent,timelineRedDarkPercent);timeline->setTimelineHues(defaultNormalHue,defaultFocusHue);
    }

    applyOverlayCellSettings();
    updateTimeLabel(position,duration);
}

void MainWindow::updateWarpOverlay(){if(mpvWidget)mpvWidget->setWarpOverlay(warpPlaybackMode,warpFactor);applyTimelineHueTheme();}

void MainWindow::setWarpFactor(int factor){warpFactor=qBound(1,factor,9);updateWarpOverlay();if(warpPlaybackMode&&mpvWidget)mpvWidget->showWarpFeedback(warpFactor);}

void MainWindow::toggleWarpPlaybackMode(){warpPlaybackMode=!warpPlaybackMode;if(warpPlaybackMode){setAutoPlayNextEnabled(false);if(mpvWidget->currentFilePath().isEmpty()&&playlistModel->count()>0)playPlaylistRow(currentRow());mpvWidget->setPause(false);if(fastPlaybackTimer)fastPlaybackTimer->start();if(mpvWidget)mpvWidget->showWarpFeedback(warpFactor);}else{if(fastPlaybackTimer)fastPlaybackTimer->stop();setAutoPlayNextEnabled(true);mpvWidget->setPause(false);mpvWidget->showPlaybackOverlay(false);}updateWarpOverlay();}

void MainWindow::fastPlaybackTick(){
    if(!warpPlaybackMode||!mpvWidget)return;
    if(mpvWidget->currentFilePath().isEmpty()){
        warpPlaybackMode=false;
        if(fastPlaybackTimer)fastPlaybackTimer->stop();
        updateWarpOverlay();
        return;
    }
    const double skipSeconds=10.0*warpFactor;
    if(duration>0.0&&position>=0.0&&(duration-position)<=skipSeconds){
        toggleWarpPlaybackMode();
        return;
    }
    mpvWidget->seekRelativeKeyframe(skipSeconds);
}

void MainWindow::updateTimeLabel(double p,double d){
    if(!timeLabel)return;
    timeLabel->setText(formatHMS(p)+QStringLiteral(" / ")+formatHMS(d));
    QColor activeColor(0,255,0);
    const bool playlistFocusNow=playlistKeyboardFocus||(playlistView&&playlistView->hasFocus())||(playlistSearchEdit&&playlistSearchEdit->hasFocus());
    if(warpPlaybackMode)activeColor=QColor(255,0,0);
    else if(playlistFocusNow)activeColor=QColor(128,128,128);
    timeLabel->setStyleSheet(QStringLiteral(
        "QLabel{background:#181818;color:rgb(%1,%2,%3);border:1px solid rgb(%1,%2,%3);padding:2px 7px;}")
        .arg(activeColor.red()).arg(activeColor.green()).arg(activeColor.blue()));
}

void MainWindow::onPositionChanged(double s){position=s;timeline->setPosition(s);updateTimeLabel(position,duration);updateScaleButtons();}

void MainWindow::onDurationChanged(double s){duration=s;timeline->setDuration(s);playlistModel->setDurationForPath(mpvWidget->currentFilePath(),s);savePlaylistState();updateTimeLabel(position,duration);}

