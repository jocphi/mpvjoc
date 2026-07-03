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
#include <QFrame>
#include <QHBoxLayout>
#include <QSettings>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QWidget>
#include <QGroupBox>
#include <QCheckBox>

void MainWindow::setPlaylistKeyboardFocus(bool focus){
    playlistKeyboardFocus=focus;
    if(focus){if(playlistView)playlistView->setFocus(Qt::TabFocusReason);}else{if(mpvWidget)mpvWidget->setFocus(Qt::TabFocusReason);} if(timeline)timeline->setPlaylistFocusMode(playlistKeyboardFocus);

    applyTimelineHueTheme();
}
void MainWindow::toggleKeyboardFocusTarget(){setPlaylistKeyboardFocus(!playlistKeyboardFocus);}

void MainWindow::updateMuteVolumeButton(){ if(!muteButton)return; muteButton->setText(QString("Vol %1%").arg(int(currentVolume+0.5))); QFont f=muteButton->font(); f.setStrikeOut(currentMuted); muteButton->setFont(f); muteButton->setToolTip(currentMuted?"Muted - click to unmute":"Click to mute"); }

void MainWindow::playPauseOrStartSelected(){ if(mpvWidget->currentFilePath().isEmpty()&&playlistModel->count()>0){playPlaylistRow(currentRow());return;} mpvWidget->togglePause();}

void MainWindow::closeCurrentFile(){if(fastPlaybackTimer)fastPlaybackTimer->stop();warpPlaybackMode=false;updateWarpOverlay();suppressNextEndFileAdvance=true;mpvWidget->stopPlayback();position=0;duration=0;timeline->setPosition(0);timeline->setDuration(0);timeline->setTitle(QString());updateTimeLabel(0,0);playPauseButton->setText("Play");}

double MainWindow::normalizedMaxVideoScale(double scale)const{ if(scale<0.75)return 0.5; if(scale<1.5)return 1.0; return 2.0;}

void MainWindow::updateScaleButtons(){ auto apply=[this](QPushButton*b,bool active){ if(!b)return; QFont f=b->font(); f.setStrikeOut(!active); b->setFont(f); b->setEnabled(true); }; apply(scaleHalfButton,maxVideoScale==0.5); apply(scaleOneButton,maxVideoScale==1.0); apply(scaleTwoButton,maxVideoScale==2.0); updateClipButton();}

void MainWindow::updateClipButton(){if(!clipButton)return;clipButton->setText(clipVideoToScale?"clip":"scale");clipButton->setToolTip(clipVideoToScale?"Clip enabled: exact scale may clip video":"Scale enabled: scale down only when needed to show full video");}

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

void MainWindow::setAutoPlayNextEnabled(bool enabled){autoPlayNextEnabled=enabled;updateAutoPlayButton();if(!restoringPlaybackState)savePlaylistState();}

void MainWindow::updateAutoPlayButton(){if(!autoPlayButton)return;autoPlayButton->setText(autoPlayNextEnabled?"auto":"manual");autoPlayButton->setToolTip(autoPlayNextEnabled?"Autoplay next playlist item enabled":"Autoplay next playlist item disabled");}



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

void MainWindow::openSettingsDialog()
{
    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("mpvjoc Settings"));
    dialog.setModal(true);

    auto* rootLayout = new QVBoxLayout(&dialog);
    rootLayout->setContentsMargins(10, 10, 10, 10);
    rootLayout->setSpacing(10);

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

    // Timeline section. More timeline settings can be added here later.
    auto* timelineGroup = new QGroupBox(QStringLiteral("Timeline"), &dialog);
    auto* timelineLayout = new QFormLayout(timelineGroup);
    timelineLayout->setContentsMargins(10, 8, 10, 10);
    timelineLayout->setSpacing(8);

    auto makeTimelineRow = [&](const QString& title, const QColor& baseColor, int currentPercent) {
        auto* row = new QWidget(timelineGroup);
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
    rootLayout->addWidget(timelineGroup);

    // Playback section. These are existing app states exposed in the settings dialog.
    auto* playbackGroup = new QGroupBox(QStringLiteral("Playback"), &dialog);
    auto* playbackLayout = new QVBoxLayout(playbackGroup);
    playbackLayout->setContentsMargins(10, 8, 10, 10);
    playbackLayout->setSpacing(6);

    auto* autoPlayCheck = new QCheckBox(QStringLiteral("Auto-play next playlist item"), playbackGroup);
    autoPlayCheck->setChecked(autoPlayNextEnabled);
    autoPlayCheck->setToolTip(QStringLiteral("When enabled, end-of-file advances to the next playlist item."));

    auto* clipCheck = new QCheckBox(QStringLiteral("Clip exact video scale when larger than the video area"), playbackGroup);
    clipCheck->setChecked(clipVideoToScale);
    clipCheck->setToolTip(QStringLiteral("When enabled, exact scale may clip video. When disabled, oversized video is scaled down to fit."));

    playbackLayout->addWidget(autoPlayCheck);
    playbackLayout->addWidget(clipCheck);
    rootLayout->addWidget(playbackGroup);

    // Playlist section. Currently informational, but deliberately placed here so
    // future playlist settings have a clear home.
    auto* playlistGroup = new QGroupBox(QStringLiteral("Playlist"), &dialog);
    auto* playlistLayout = new QVBoxLayout(playlistGroup);
    playlistLayout->setContentsMargins(10, 8, 10, 10);
    playlistLayout->setSpacing(6);
    auto* searchHint = new QLabel(
        QStringLiteral("Search syntax: words are AND terms, prefix with - to exclude, use reviewed/watched/seen for reviewed rows."),
        playlistGroup);
    searchHint->setWordWrap(true);
    playlistLayout->addWidget(searchHint);
    rootLayout->addWidget(playlistGroup);

    rootLayout->addStretch(1);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    rootLayout->addWidget(buttons);

    if (dialog.exec() == QDialog::Accepted) {
        timelineGreenDarkPercent = greenSpin->value();
        timelineGreyDarkPercent = greySpin->value();
        timelineRedDarkPercent = redSpin->value();
        saveTimelineColorSettings();
        applyTimelineHueTheme();

        setAutoPlayNextEnabled(autoPlayCheck->isChecked());
        setClipVideoToScale(clipCheck->isChecked());
        savePlaylistState();
    }
}



void MainWindow::applyTimelineHueTheme(){
    if(!timeline)return;
    static bool timelineColorSettingsLoaded=false;
    if(!timelineColorSettingsLoaded){loadTimelineColorSettings();timelineColorSettingsLoaded=true;}
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

void MainWindow::updateTimeLabel(double p,double d){if(timeLabel)timeLabel->setText(formatHMS(p)+" / "+formatHMS(d));}

void MainWindow::onPositionChanged(double s){position=s;timeline->setPosition(s);updateTimeLabel(position,duration);}

void MainWindow::onDurationChanged(double s){duration=s;timeline->setDuration(s);playlistModel->setDurationForPath(mpvWidget->currentFilePath(),s);savePlaylistState();updateTimeLabel(position,duration);}

