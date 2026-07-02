#include "MainWindow.h"
#include "TimelineWidget.h"
#include "mpv/MpvWidget.h"
#include "playlist/PlaylistModel.h"
#include "util/Utils.h"
#include <QFont>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QtGlobal>

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
        f.setPointSize(10);
        shortcutHelpOverlay->setFont(f);
        shortcutHelpOverlay->setStyleSheet(QStringLiteral(
            "QLabel{background:rgba(0,0,0,215);color:#eeeeee;border:2px solid #66ccff;border-radius:8px;padding:14px;}"));
        shortcutHelpOverlay->setText(QStringLiteral(
            "Keyboard shortcuts\n"
            "\n"
            "Playback\n"
            "  P          Play/Pause, or leave Warp mode if active\n"
            "  Ctrl+P     Toggle Warp mode\n"
            "  Left/Right Change Warp factor while Warp mode is active\n"
            "  F          Next frame\n"
            "  D          Previous frame\n"
            "  M          Toggle mute\n"
            "  + / -      Volume up/down\n"
            "\n"
            "Scale / display\n"
            "  1/2        Set scale to 50% with the 1/2 key\n"
            "  1          Set scale to 100%\n"
            "  2          Set scale to 200%\n"
            "  0          Toggle clip/scale\n"
            "  F1         Toggle persistent info overlay\n"
            "  F12        Show/hide this help\n"
            "  Esc        Hide this help\n"
            "\n"
            "Playlist\n"
            "  Enter      Play selected\n"
            "  PageDown   Next item\n"
            "  PageUp     Previous item\n"
            "  Delete     Remove from playlist\n"
            "  Shift+Del  Move file to trash\n"
            "  Ctrl+Del   Clear playlist\n"
            "  Alt+Up     Move selected item up\n"
            "  Alt+Down   Move selected item down\n"
            "\n"
            "Seeking\n"
            "  .          Forward 60 seconds\n"
            "  ,          Back 60 seconds\n"
            "  Ctrl+.     Forward 10 seconds\n"
            "  Ctrl+,     Back 10 seconds\n"
            "  :          Forward 30 seconds\n"
            "  ;          Back 30 seconds"));
    }
    const int w=qMin(width()-40,620);
    const int h=qMin(height()-40,520);
    shortcutHelpOverlay->setGeometry((width()-w)/2,(height()-h)/2,w,h);
    shortcutHelpOverlay->show();
    shortcutHelpOverlay->raise();
}

void MainWindow::setMaxVideoScale(double scale){ maxVideoScale=normalizedMaxVideoScale(scale); if(mpvWidget){mpvWidget->setMaxVideoScale(maxVideoScale);mpvWidget->showScaleOverlay();} updateScaleButtons(); if(!restoringPlaybackState)savePlaylistState();}

void MainWindow::setClipVideoToScale(bool crop){clipVideoToScale=crop;if(mpvWidget){mpvWidget->setClipVideoToScale(clipVideoToScale);mpvWidget->showScaleOverlay();}updateClipButton();if(!restoringPlaybackState)savePlaylistState();}

void MainWindow::setAutoPlayNextEnabled(bool enabled){autoPlayNextEnabled=enabled;updateAutoPlayButton();if(!restoringPlaybackState)savePlaylistState();}

void MainWindow::updateAutoPlayButton(){if(!autoPlayButton)return;autoPlayButton->setText(autoPlayNextEnabled?"auto":"manual");autoPlayButton->setToolTip(autoPlayNextEnabled?"Autoplay next playlist item enabled":"Autoplay next playlist item disabled");}

void MainWindow::updateWarpOverlay(){if(mpvWidget)mpvWidget->setWarpOverlay(warpPlaybackMode,warpFactor);}

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

