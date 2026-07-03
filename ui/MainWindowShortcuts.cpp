#include "MainWindow.h"
#include "mpv/MpvWidget.h"
#include "playlist/PlaylistModel.h"
#include <QKeyEvent>
#include <QLineEdit>
#include <QLabel>
#include <QListView>
#include <QString>

QString MainWindow::shortcutHelpText()const{
    return QStringLiteral(
        "Keyboard shortcuts\n"
        "\n"
        "Playback / Warp\n"
        "  P          Play/Pause, or leave Warp mode\n"
        "  Ctrl+P     Toggle Warp mode\n"
        "  Q          Quit application\n"
        "  0/1/2/3/4/5/6/7/½    Exact video scale presets\n"
        "  Left/Right Change Warp factor while Warp is active\n"
        "  F / D      Next / previous frame\n"
        "  M          Toggle mute\n"
        "  + / -      Volume up/down\n"
        "\n"
        "Scale / display\n"
        "  1/2        Set scale to 50% with the 1/2 key\n"
        "  1 / 2      Set scale to 100% / 200%\n"
        "  0          Toggle clip/scale\n"
        "  F1         Toggle persistent info overlay\n"
        "  F12        Show/hide this help\n"
        "  Esc        Hide this help / clear search\n"
        "\n"
        "Playlist / media\n"
        "  Ctrl+F     Focus/select playlist search/filter\n"
        "  Ctrl+O     Open files\n"
        "  F4         Close current file\n"
        "  F5         Refresh media metadata/scans\n"
        "  Ctrl+F5    Settings\n"
        "  Esc        Clear search and return to playlist\n"
        "  Search     words = AND, -term = exclude, reviewed = reviewed rows\n"
        "  W          Toggle reviewed marker\n"
        "  Tab        Toggle playlist/video keyboard focus\n"
        "  Letters/numbers use global shortcuts, not playlist type-ahead\n"
        "  F5         Refresh media metadata/scans\n"
        "  Ctrl+F5    Settings\n"
        "  Enter      Play selected\n"
        "  PageDown   Next item\n"
        "  PageUp     Previous item\n"
        "  Delete     Remove from playlist\n"
        "  Shift+Del  Move file to trash\n"
        "  Ctrl+Del   Clear playlist\n"
        "  Alt+Up/Dn  Move selected item up/down\n"
        "\n"
        "Seeking\n"
        "  . / ,      Forward / back 60 seconds\n"
        "  Ctrl+./,   Forward / back 10 seconds\n"
        "  : / ;      Forward / back 30 seconds");
}

void MainWindow::keyPressEvent(QKeyEvent*e){auto mods=e->modifiers(); int k=e->key(); if(e->isAutoRepeat()&&(k==Qt::Key_PageDown||k==Qt::Key_PageUp)){e->accept();return;} int moveShortcut=-1; if((mods&Qt::ShiftModifier)&&k>=Qt::Key_1&&k<=Qt::Key_6)moveShortcut=k-Qt::Key_1; else if(k==Qt::Key_Exclam)moveShortcut=0; else if(k==Qt::Key_QuoteDbl)moveShortcut=1; else if(k==Qt::Key_NumberSign)moveShortcut=2; else if(k==Qt::Key_currency)moveShortcut=3; else if(k==Qt::Key_Percent)moveShortcut=4; else if(k==Qt::Key_Ampersand)moveShortcut=5; if(moveShortcut>=0){moveSelectedFileToTarget(moveShortcut);e->accept();return;} if(k==Qt::Key_Tab&&((mods&~Qt::KeypadModifier)==Qt::NoModifier)){toggleKeyboardFocusTarget();e->accept();return;} if(k==Qt::Key_Escape&&shortcutHelpOverlay&&shortcutHelpOverlay->isVisible()){shortcutHelpOverlay->hide();e->accept();return;} if(k==Qt::Key_Escape&&playlistSearchEdit&&(playlistSearchEdit->hasFocus()||!playlistSearchEdit->text().isEmpty())){playlistSearchEdit->clear();playlistSearchEdit->clearFocus();applyTimelineHueTheme();e->accept();return;} if(k==Qt::Key_F&&(mods&Qt::ControlModifier)){if(playlistSearchEdit){playlistSearchEdit->setFocus(Qt::ShortcutFocusReason);playlistSearchEdit->selectAll();applyTimelineHueTheme();}e->accept();return;} if(k==Qt::Key_F12){toggleShortcutHelpOverlay();e->accept();return;} if(k==Qt::Key_F1){mpvWidget->toggleInfoOverlay();e->accept();return;} bool plainScaleShortcut=((mods&~Qt::KeypadModifier)==Qt::NoModifier); if(plainScaleShortcut&&k==Qt::Key_0){setClipVideoToScale(!clipVideoToScale);e->accept();return;} if(plainScaleShortcut&&e->text()==QString::fromUtf8("½")){setMaxVideoScale(0.5);e->accept();return;} if(plainScaleShortcut&&k==Qt::Key_1){setMaxVideoScale(1.0);e->accept();return;} if(plainScaleShortcut&&k==Qt::Key_2){setMaxVideoScale(2.0);e->accept();return;} if(plainScaleShortcut&&k==Qt::Key_F){mpvWidget->frameStepForward();e->accept();return;} if(plainScaleShortcut&&k==Qt::Key_D){mpvWidget->frameStepBackward();e->accept();return;} if(k==Qt::Key_P&&(mods&Qt::ControlModifier)){toggleWarpPlaybackMode();e->accept();return;} if(k==Qt::Key_P){ if(warpPlaybackMode){toggleWarpPlaybackMode();e->accept();return;} if(playlistView->hasFocus())playPlaylistRow(currentRow()); else playPauseOrStartSelected(); e->accept();return;} if(k==Qt::Key_Return||k==Qt::Key_Enter){playPlaylistRow(currentRow());e->accept();return;} if(k==Qt::Key_Delete&&(mods&Qt::ControlModifier)){clearPlaylist();e->accept();return;} if(k==Qt::Key_Delete&&(mods&Qt::ShiftModifier)){moveSelectedFileToTrash();e->accept();return;} if(k==Qt::Key_Delete){removeSelectedItem();e->accept();return;} if(k==Qt::Key_Up&&(mods&Qt::AltModifier)){moveSelectedItemUp();e->accept();return;} if(k==Qt::Key_Down&&(mods&Qt::AltModifier)){moveSelectedItemDown();e->accept();return;} if(warpPlaybackMode&&k==Qt::Key_Right){setWarpFactor(warpFactor+1);e->accept();return;} if(warpPlaybackMode&&k==Qt::Key_Left){setWarpFactor(warpFactor-1);e->accept();return;} if(k==Qt::Key_PageDown){playNextPlaylistFile();e->accept();return;} if(k==Qt::Key_PageUp){playPreviousPlaylistFile();e->accept();return;} if(k==Qt::Key_Period&&(mods&Qt::ControlModifier)){mpvWidget->seekRelative(10);e->accept();return;} if(k==Qt::Key_Comma&&(mods&Qt::ControlModifier)){mpvWidget->seekRelative(-10);e->accept();return;} if(k==Qt::Key_Period){mpvWidget->seekRelative(60);e->accept();return;} if(k==Qt::Key_Comma){mpvWidget->seekRelative(-60);e->accept();return;} if(k==Qt::Key_Colon){mpvWidget->seekRelative(30);e->accept();return;} if(k==Qt::Key_Semicolon){mpvWidget->seekRelative(-30);e->accept();return;} if(k==Qt::Key_M){mpvWidget->toggleMute();e->accept();return;} if(k==Qt::Key_Plus){mpvWidget->changeVolume(5);e->accept();return;} if(k==Qt::Key_Minus){mpvWidget->changeVolume(-5);e->accept();return;} if(k==Qt::Key_O&&(mods&Qt::ControlModifier)){openFiles();e->accept();return;} if(k==Qt::Key_F4){closeCurrentFile();e->accept();return;} if(k==Qt::Key_F5&&(mods&Qt::ControlModifier)){openSettingsDialog();e->accept();return;} if(k==Qt::Key_F5){refreshMediaScans();e->accept();return;} if(k==Qt::Key_W&&((mods&~Qt::KeypadModifier)==Qt::NoModifier)){toggleReviewedForCurrent();e->accept();return;} QMainWindow::keyPressEvent(e);}
