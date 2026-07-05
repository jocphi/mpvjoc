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
#include <QComboBox>
#include <QSignalBlocker>
#include <QSet>
#include <QList>
#include <QPair>

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

    auto* overlayGroup = new QGroupBox(QStringLiteral("Overlay layout"), &dialog);
    auto* overlayLayout = new QFormLayout(overlayGroup);
    overlayLayout->setContentsMargins(10, 8, 10, 10);
    overlayLayout->setSpacing(8);
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
        auto* rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        rowLayout->setSpacing(8);
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
    rootLayout->addWidget(overlayGroup);

    auto* profilesGroup = new QGroupBox(QStringLiteral("Overlay profiles"), &dialog);
    auto* profilesLayout = new QVBoxLayout(profilesGroup);
    profilesLayout->setContentsMargins(10, 8, 10, 10);
    profilesLayout->setSpacing(8);
    auto* profilesHint = new QLabel(QStringLiteral("Choose which overlay elements are shown for each overlay mode."), profilesGroup);
    profilesHint->setWordWrap(true);
    profilesLayout->addWidget(profilesHint);

    auto makeProfileRow = [&](const QString& title, int mask) {
        auto* row = new QWidget(profilesGroup);
        auto* rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        rowLayout->setSpacing(8);
        auto* titleLabel = new QLabel(title, row);
        titleLabel->setMinimumWidth(135);
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
    rootLayout->addWidget(profilesGroup);

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

    auto* playlistGroup = new QGroupBox(QStringLiteral("Playlist"), &dialog);
    auto* playlistLayout = new QVBoxLayout(playlistGroup);
    playlistLayout->setContentsMargins(10, 8, 10, 10);
    playlistLayout->setSpacing(6);
    auto* searchHint = new QLabel(QStringLiteral("Search syntax: words are AND terms, prefix with - to exclude, use reviewed/watched/seen for reviewed rows."), playlistGroup);
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
        setAutoPlayNextEnabled(autoPlayCheck->isChecked());
        setClipVideoToScale(clipCheck->isChecked());
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

