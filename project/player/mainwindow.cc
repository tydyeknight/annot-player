// mainwindow.cc
// 6/30/2011

#include "mainwindow.h"
#include "mainwindowprivate.h"
#include "application.h"

#include "db.h"
#include "datamanager.h"
#include "dataserver.h"
#include "logger.h"
#include "eventlogger.h"
#include "grabber.h"
#include "osdconsole.h"
#include "osdwindow.h"
#include "playerui.h"
#include "videoview.h"
#include "embeddedplayer.h"
#include "signalhub.h"
#include "mainplayer.h"
#include "miniplayer.h"
#include "annotationgraphicsview.h"
#include "annotationeditor.h"
#include "annotationfilter.h"
#include "tokenview.h"
//#include "commentview.h"
#include "tray.h"
//#include "cloudview.h"
#include "annotationbrowser.h"
#include "stylesheet.h"
#include "uistyle.h"
#include "rc.h"
#include "tr.h"
#include "translatormanager.h"
#include "settings.h"
#include "siteaccountview.h"
#include "aboutdialog.h"
#include "annotationcountdialog.h"
#include "blacklistview.h"
#include "backlogdialog.h"
#include "downloaddialog.h"
#include "consoledialog.h"
#include "devicedialog.h"
#include "helpdialog.h"
#include "logindialog.h"
#include "pickdialog.h"
#include "seekdialog.h"
#include "livedialog.h"
#include "syncdialog.h"
#include "mediaurldialog.h"
#include "suburldialog.h"
#include "userview.h"
#include "defines.h"
#ifdef USE_MODE_SIGNAL
  #include "signalview.h"
  #include "messageview.h"
  #include "processview.h"
  #include "messagehandler.h"
#endif // USE_MODE_SIGNAL
#include "module/player/player.h"
#include "module/annotcodec/annotationcodecmanager.h"
#include "module/mrlresolver/mrlresolvermanager.h"
#include "module/mrlresolver/mrlresolversettings.h"
#include "module/translator/translator.h"
#include "module/qtext/actionwithid.h"
#include "module/qtext/countdowntimer.h"
#include "module/qtext/datetime.h"
#include "module/qtext/htmltag.h"
//#include "module/qtext/network.h"
#include "module/mediacodec/flvcodec.h"
#ifdef USE_MODULE_SERVERAGENT
  #include "module/serveragent/serveragent.h"
#endif // USE_MODULE_SERVERAGENT
#ifdef USE_MODULE_CLIENTAGENT
  #include "module/clientagent/clientagent.h"
#endif // USE_MODULE_CLIENTAGENT
#ifdef USE_MODULE_DOLL
  #include "module/doll/doll.h"
#endif // USE_MODULE_DOLL
#ifdef USE_WIN_HOOK
  #include "win/hook/hook.h"
#endif // USE_WIN_HOOK
#ifdef USE_WIN_MOUSEHOOK
  #include "win/mousehook/mousehook.h"
#endif // USE_WIN_MOUSEHOOK
//#ifdef USE_WIN_DWM
//  #include "win/dwm/dwm.h"
//#endif // USE_WIN_DWM
#ifdef USE_WIN_QTH
  #include "win/qth/qth.h"
#endif // USE_WIN_QTH
#ifdef Q_WS_WIN
  #include "win/qtwin/qtwin.h"
#endif // Q_WS_WIN
#ifdef USE_WIN_PICKER
  #include "win/picker/picker.h"
#endif // USE_WIN_PICKER
#ifdef Q_WS_MAC
  #include "mac/qtmac/qtmac.h"
  //#include "mac/qtstep/qtstep.h"
  //#include "mac/qtvlc/qtvlc.h"
#endif // Q_WS_MAC
#ifdef Q_WS_X11
  #include "unix/qtx/qtx.h"
#endif // Q_WS_X11
#ifdef Q_OS_UNIX
  #include "unix/qtunix/qtunix.h"
#endif // Q_OS_UNIX
#include "module/annotcloud/cmd.h"
#include "module/annotcloud/annotationparser.h"
#include <QtGui>
//#include <QtNetwork>
#include <boost/foreach.hpp>
#include <boost/tuple/tuple.hpp>
#include <boost/typeof/typeof.hpp>
#include <cstring>
#include <ctime>

#ifndef USE_MODULE_SERVERAGENT
  #error "Module server agent is indispensible."
#endif // USE_MODULE_SERVERAGENT

using namespace AnnotCloud;
using namespace Logger;

#define DEBUG "mainwindow"
#include "module/debug/debug.h"

#define DEFAULT_LIVE_INTERVAL   3000    // 3 seconds

// - Focus -

void
MainWindow::onFocusedWidgetChanged(QWidget *w_old, QWidget *w_new)
{
#ifdef Q_WS_WIN
  // When EmbeddedPlayer's lineEdit cleared its focus.
  if (!w_new && w_old == embeddedPlayer_->inputComboBox())
    QtWin::sendMouseClick(QPoint(0, 0), Qt::LeftButton);
#else
  Q_UNUSED(w_old);
  Q_UNUSED(w_new);
#endif // Q_WS_WIN
}

bool
MainWindow::isEditing() const
{
  return inputLineHasFocus()
      && prefixLineHasFocus();
}

bool
MainWindow::inputLineHasFocus() const
{
  return mainPlayer_->inputComboBox()->hasFocus()
      || miniPlayer_->inputComboBox()->hasFocus()
      || embeddedPlayer_->inputComboBox()->hasFocus();
}

bool
MainWindow::prefixLineHasFocus() const
{
  return mainPlayer_->prefixComboBox()->hasFocus()
      || miniPlayer_->prefixComboBox()->hasFocus()
      || embeddedPlayer_->prefixComboBox()->hasFocus();
}

// - Constructions -

void
MainWindow::resetPlayer()
{
  Q_ASSERT(player_);
  player_->reset();

#ifdef Q_WS_WIN
  player_->setMouseEventEnabled();
#endif // Q_WS_WIN
  //player_->setKeyboardEnabled(true);  // default true
  //player_->setMouseEnabled(true);     // default true
  //player_->setEncoding("UTF-8");      // default do nothing

#ifdef Q_WS_MAC
  videoView_->showView();
  player_->setEmbeddedWindow(videoView_->view());
  QTimer::singleShot(0, videoView_, SLOT(hideView()));
  //videoView_->hideView();
#else
  player_->embed(videoView_);
#endif // Q_WS_MAC
  player_->setFullScreen(false);
}

MainWindow::MainWindow(QWidget *parent, Qt::WindowFlags f)
  : Base(parent, f), disposed_(false),
    liveInterval_(DEFAULT_LIVE_INTERVAL),
    tray_(0),//commentView_(0),
    annotationEditor_(0), blacklistView_(0), cloudView_(0), backlogDialog_(0), consoleDialog_(0), downloadDialog_(0),
    aboutDialog_(0), annotationCountDialog_(0), deviceDialog_(0), helpDialog_(0), loginDialog_(0),
    processPickDialog_(0), seekDialog_(0), syncDialog_(0), windowPickDialog_(0),
    mediaUrlDialog_(0), annotationUrlDialog_(0), siteAccountView_(0), userView_(0),
    dragPos_(BAD_POS), tokenType_(0), recentSourceLocked_(false),
    themeMenu_(0),
    setThemeToDefaultAct_(0), setThemeToRandomAct_(0),
    setThemeToDarkAct_(0), setThemeToBlackAct_(0), setThemeToBlueAct_(0),
    setThemeToBrownAct_(0), setThemeToCyanAct_(0), setThemeToGrayAct_(0),
    setThemeToGreenAct_(0), setThemeToPinkAct_(0), setThemeToPurpleAct_(0),
    setThemeToRedAct_(0), setThemeToWhiteAct_(0), setThemeToYellowAct_(0)
{
  //setWindowFlags(windowFlags() | Qt::WindowStaysOnTopHint);
//#ifdef Q_WS_MAC
//  setWindowFlags(windowFlags() | Qt::FramelessWindowHint);
//#endif // Q_WS_MAC

  setWindowIcon(QIcon(RC_IMAGE_APP)); // X11 require setting icon at runtime
  setWindowTitle(TR(T_TITLE_PROGRAM));
  setContentsMargins(0, 0, 0, 0);
  UiStyle::globalInstance()->setMainWindowStyle(this);

  menuBar()->setVisible(Settings::globalInstance()->isMenuBarVisible());

  createComponents();

  createConnections();

  createActions();

  createMenus();

  createDockWindows();

  // - Post behaviors

  // Accept drag/drop behavior (default enabled on Windows but disabled on Mac)
  setAcceptDrops(true);

  // Receive mouseMoveEvent even no mouse button is clicked
  // Set this to enable resetAutoHide embeddedPlayer when mouse is moved.
  setMouseTracking(true);

  // Adjust EmbeddedPlayer to fit in OsdWindow.
  embeddedPlayer_->invalidateGeometry();

  // Global hooks
#ifdef USE_WIN_HOOK
  HOOK->setMouseHookEnabled(true);
  //HOOK->setKeyboardHookEnabled(false);
  HOOK->setThreadsFilterEnabled(false);         // Use global hook.
  HOOK->setWindowsFilterEnabled(true);          // Only enable for specified window
  HOOK->setContextMenuEventEnabled(true);       // Listen to context menu event.
  HOOK->setDoubleClickEventEnabled(true);       // Listen to double click event.
  HOOK->setMouseMoveEventEscaped(true);         // Always receive mouse move event
  HOOK->installEventFilter(new HookEventForwarder(this));

  //connect(player_, SIGNAL(mediaOpen()), videoView_, SLOT(addToWindowsHook()));
  connect(player_, SIGNAL(playing()), videoView_, SLOT(addToWindowsHook()));
  connect(player_, SIGNAL(mediaClosed()), videoView_, SLOT(removeFromWindowsHook()));

  HOOK->start();
#endif // USE_WIN_HOOK

#ifdef USE_WIN_MOUSEHOOK
  MouseHook::globalInstance()->setEventListener(this);
  MouseHook::globalInstance()->start();
#endif // USE_WIN_MOUSEHOOK

  // Load settings
  Settings *settings = Settings::globalInstance();

  setSubtitleColor(settings->subtitleColor());

  recentFiles_ = settings->recentFiles();
  invalidateRecentMenu();

  recentPath_ = settings->recentPath();

  playPosHistory_ = settings->playPosHistory();
  subtitleHistory_ = settings->subtitleHistory();
  audioTrackHistory_= settings->audioTrackHistory();

  setTranslateEnabled(settings->isTranslateEnabled());
  setSubtitleOnTop(settings->isSubtitleOnTop());

  embeddedPlayer_->setOnTop(settings->isEmbeddedPlayerOnTop());

  annotationView_->setRenderHint(settings->annotationEffect());

  annotationFilter_->setEnabled(settings->isAnnotationFilterEnabled());
  annotationFilter_->setBlockedTexts(settings->blockedKeywords());
  annotationFilter_->setBlockedUserAliases(settings->blockedUserNames());
  //annotationFilter_->setAnnotationCountHint(settings->annotationCountHint());
  {
    qint64 l = settings->annotationLanguages();
    annotationFilter_->setLanguages(l);

    toggleAnnotationLanguageToAnyAct_->setChecked(l & Traits::AnyLanguageBit);
    toggleAnnotationLanguageToUnknownAct_->setChecked(l & Traits::UnknownLanguageBit);
    toggleAnnotationLanguageToEnglishAct_->setChecked(l & Traits::EnglishBit);
    toggleAnnotationLanguageToJapaneseAct_->setChecked(l & Traits::JapaneseBit);
    toggleAnnotationLanguageToChineseAct_->setChecked(l & Traits::ChineseBit);
    toggleAnnotationLanguageToKoreanAct_->setChecked(l & Traits::KoreanBit);
    invalidateAnnotationLanguages();
  }

  {
    MrlResolverSettings *mrs= MrlResolverSettings::globalInstance();
    std::pair<QString, QString>
    a = settings->nicovideoAccount();
    if (!a.first.isEmpty() && !a.second.isEmpty())
      mrs->setNicovideoAccount(a.first, a.second);

    a = settings->bilibiliAccount();
    if (!a.first.isEmpty() && !a.second.isEmpty())
      mrs->setBilibiliAccount(a.first, a.second);
  }

#ifdef Q_WS_WIN
  //connect(annotationView_, SIGNAL(posChanged()), SLOT(ensureWindowOnTop()));
#endif // Q_WS_WIN

#ifdef Q_WS_MAC
  // Forward reset player on Mac OS X
  if (!player_->isValid())
    resetPlayer();
#endif // Q_WS_MAC

  // Initial focus
  mainPlayer_->inputComboBox()->setFocus();

  // System tray icon
  if (tray_)
    tray_->show(); // FIXME: not working on mac?

  // Parse arguments
  //parseArguments(args);

  //statusBar()->showMessage(tr("Ready"));
}

void
MainWindow::createComponents()
{
  // Systemt tray icon
  if (QSystemTrayIcon::isSystemTrayAvailable())
    tray_ = new Tray(this);

  // Utilities
  grabber_ = new Grabber(this); {
    QString desktopPath = QDesktopServices::storageLocation(QDesktopServices::DesktopLocation);
    if (!desktopPath.isEmpty())
      grabber_->setSavePath(desktopPath);
  }

  // Caches
  cache_ = new Database(G_PATH_CACHEDB, this);
  if (!cache_->isValid())
    cache_->clear();
  queue_ = new Database(G_PATH_QUEUEDB, this);
  if (!queue_->isValid())
    queue_->clear();

  // Server agents
  server_ = new ServerAgent(this);
#ifdef USE_MODULE_CLIENTAGENT
  client_ = new ClientAgent(this);

  server_->setClientAgent(client_);
  client_->setServerAgent(server_);
#endif // USE_MODULE_CLIENTAGENT

  // Data server
  dataServer_ = new DataServer(server_, cache_, queue_, this);

  // Data manager
  dataManager_ = new DataManager(this);

  // Mrl resolver
  mrlResolver_ = new MrlResolverManager(this);

  // Qth
#ifdef USE_WIN_QTH
  //Qth *qth = new Qth(winId(), this);
  //Qth::setGlobalInstance(qth);
  //QTH->setParentWinId(winId());
#endif // USE_WIN_QTH

  // Player
  player_ = new Player(this);

  // Signal hub
  hub_ = new SignalHub(player_, this);

  // Logger
  logger_ = new EventLogger(player_, this);

  // Players and graphic views
  videoView_ = new VideoView(this);
  osdWindow_ = new OsdWindow(this);

  annotationView_ = new AnnotationGraphicsView(hub_,  player_, videoView_, osdWindow_);
  annotationView_->setFullScreenView(osdWindow_);
//#ifdef USE_WIN_DWM
//  {
//    QWidget *w = annotationView_->editor();
//    w->setParent(this, w->windowFlags()); // hot fix for dwm displaying issue
//    UiStyle::globalInstance()->setWindowDwmEnabled(w->winId(), false);
//    UiStyle::globalInstance()->setWindowDwmEnabled(w->winId(), true);
//  }
//#endif // USE_WIN_DWM
//#ifdef Q_WS_MAC // remove background in annotation view
//  annotationView_->setStyleSheet(
//    SS_BEGIN(QWidget)
//      SS_TRANSPARENT
//    SS_END
//  );
//#endif // Q_WS_MAC
  annotationFilter_ = new AnnotationFilter(dataManager_, this);
  annotationView_->setFilter(annotationFilter_);

  //QLayout *layout = new QStackedLayout(osdWindow_);
  //layout->addWidget(annotationView_);
  //osdWindow_->setLayout(layout);
  osdWindow_->setEventListener(annotationView_);

  //embeddedPlayer_ = new EmbeddedPlayerUi(hub_, player_, server_, osdWindow_);
  embeddedPlayer_ = new EmbeddedPlayerUi(hub_, player_, server_);
//#ifdef Q_WS_MAC // remove background in Osd player
//  embeddedPlayer_->setStyleSheet(
//    SS_BEGIN(QWidget)
//      SS_TRANSPARENT
//    SS_END
//  );
//#endif // Q_WS_MAC

  globalOsdConsole_ = new OsdConsole(annotationView_);
  globalOsdConsole_->setAutoClearInterval(G_CONSOLE_AUTOClEAR_TIMEOUT);
  OsdConsole::setGlobalInstance(globalOsdConsole_);

  // Show OSD window
  osdWindow_->showInOsdMode(); // this must go after all children of osdWindow_ are created
  embeddedPlayer_->setParent(osdWindow_); // Set parent after osdWindow_ become visible.
  embeddedPlayer_->setAutoHideEnabled();
  embeddedPlayer_->setContainerWidget(this);

  annotationView_->invalidateGeometry();

  mainPlayer_ = new MainPlayerUi(hub_, player_, server_, this);
  miniPlayer_ = new MiniPlayerUi(hub_, player_, server_, this);

//#ifdef Q_WS_MAC
//  videoView_->showView();
//  player_->setEmbeddedWindow(videoView_->view());
//  QTimer::singleShot(0, videoView_, SLOT(hideView()));
//#else
//  player_->embed(videoView_);
//#endif // Q_WS_MAC
//  player_->setFullScreen(false);

  // Translator
  translator_ = new Translator(this);

  // Dialogs

  //userPanel_ = new UserPanel(this);
  //userPanel_->hide(); // TODO!!!!!!

  tokenView_ = new TokenView(server_, this);
  annotationBrowser_ = new AnnotationBrowser(hub_, this);

#ifdef USE_MODE_SIGNAL
  messageHandler_ = new MessageHandler(this);
  signalView_ = new SignalView(this);
  recentMessageView_ = new MessageView(this);

  connect(this, SIGNAL(attached(ProcessInfo)), signalView_->processView(), SIGNAL(attached(ProcessInfo)));
  connect(this, SIGNAL(detached(ProcessInfo)), signalView_->processView(), SIGNAL(detached(ProcessInfo)));

  connect(signalView_->processView(), SIGNAL(attached(ProcessInfo)), recentMessageView_, SLOT(setProcessNameFromProcessInfo(ProcessInfo)));
  connect(signalView_->processView(), SIGNAL(detached(ProcessInfo)), recentMessageView_, SLOT(clearProcessName()));
#endif // USE_MODE_SIGNAL

#ifdef USE_MODULE_DOLL
  doll_ = new Doll(this);
  connect(doll_, SIGNAL(said(QString)), SLOT(respond(QString)));
#endif // USE_MODULE_DOLL

  windowStaysOnTopTimer_ = new QTimer(this);
  windowStaysOnTopTimer_->setInterval(G_WINDOWONTOP_TIMEOUT);
  connect(windowStaysOnTopTimer_, SIGNAL(timeout()), SLOT(setWindowOnTop()));

  liveTimer_ = new QTimer(this);
  connect(liveTimer_, SIGNAL(timeout()), SLOT(updateLiveAnnotations()));

  resumePlayTimer_ = new QtExt::CountdownTimer;
  resumePlayTimer_->setInterval(3000); // 3 seconds

  resumeSubtitleTimer_ = new QtExt::CountdownTimer;
  resumeSubtitleTimer_->setInterval(3000); // 3 seconds

  resumeAudioTrackTimer_ = new QtExt::CountdownTimer;
  resumeAudioTrackTimer_->setInterval(3000); // 3 seconds
}

void
MainWindow::createConnections()
{
#ifdef Q_WS_WIN
  connect(qApp, SIGNAL(focusChanged(QWidget*, QWidget*)), SLOT(onFocusedWidgetChanged(QWidget*, QWidget*)));
#endif // Q_WS_WIN

  // Player
  connect(player_, SIGNAL(mediaChanged()), annotationBrowser_, SLOT(clear()));
  connect(player_, SIGNAL(titleIdChanged(int)), SLOT(invalidateToken()));
  connect(player_, SIGNAL(errorEncountered()), SLOT(handlePlayerError()));
  connect(player_, SIGNAL(trackNumberChanged(int)), SLOT(invalidateMediaAndPlay()));
  connect(player_, SIGNAL(endReached()), SLOT(autoPlayNext()));
  connect(player_, SIGNAL(mediaTitleChanged(QString)), SLOT(setWindowTitle(QString)));

  // Resume
  connect(player_, SIGNAL(stopping()), SLOT(rememberPlayPos()));
  connect(player_, SIGNAL(stopping()), SLOT(rememberSubtitle()));
  connect(player_, SIGNAL(stopping()), SLOT(rememberAudioTrack()));

  connect(resumePlayTimer_, SIGNAL(timeout()), SLOT(resumePlayPos()));
  connect(resumeSubtitleTimer_, SIGNAL(timeout()), SLOT(resumeSubtitle()));
  connect(resumeAudioTrackTimer_, SIGNAL(timeout()), SLOT(resumeAudioTrack()));

  // Hub
  connect(hub_, SIGNAL(playModeChanged(SignalHub::PlayMode)), SLOT(updatePlayMode()));
  connect(hub_, SIGNAL(playerModeChanged(SignalHub::PlayerMode)), SLOT(updatePlayerMode()));
  connect(hub_, SIGNAL(tokenModeChanged(SignalHub::TokenMode)), SLOT(updateTokenMode()));
  connect(hub_, SIGNAL(windowModeChanged(SignalHub::WindowMode)), SLOT(updateWindowMode()));
  connect(hub_, SIGNAL(opened()),SLOT(open()));
  connect(hub_, SIGNAL(played()),SLOT(play()));
  connect(hub_, SIGNAL(paused()),SLOT(pause()));
  connect(hub_, SIGNAL(stopped()),SLOT(stop()));

  // Player UI

  #define CONNECT(_playerui) \
    connect(_playerui, SIGNAL(textEntered(QString)), SLOT(eval(QString))); \
    connect(_playerui, SIGNAL(loginRequested()), SLOT(showLoginDialog())); \
    connect(_playerui, SIGNAL(showPositionPanelRequested()), SLOT(showSeekDialog())); \
    connect(_playerui->inputComboBox()->lineEdit(), SIGNAL(textChanged(QString)), SLOT(syncInputLineText(QString))); \
    connect(_playerui->prefixComboBox()->lineEdit(), SIGNAL(textChanged(QString)), SLOT(syncPrefixLineText(QString))); \
    connect(_playerui->toggleAnnotationButton(), SIGNAL(clicked()), annotationView_, SLOT(toggleVisible())); \
    connect(_playerui->previousButton(), SIGNAL(clicked()), SLOT(previous())); \
    connect(_playerui->nextButton(), SIGNAL(clicked()), SLOT(next())); \
    connect(_playerui, SIGNAL(invalidateMenuRequested()), SLOT(invalidateContextMenu())); \
    connect(annotationView_, SIGNAL(visibleChanged(bool)), _playerui, SLOT(setAnnotationEnabled(bool)));

    CONNECT(mainPlayer_)
    CONNECT(miniPlayer_)
    CONNECT(embeddedPlayer_)
  #undef CONNECT

  connect(embeddedPlayer_, SIGNAL(invalidateMenuRequested()), SLOT(invalidateContextMenu()));

  // Queued connections
  connect(this, SIGNAL(responded(QString)), SLOT(respond(QString)), Qt::QueuedConnection);
  connect(this, SIGNAL(said(QString,QString)), SLOT(say(QString,QString)), Qt::QueuedConnection);
  connect(this, SIGNAL(showTextRequested(QString)), SLOT(showText(QString)), Qt::QueuedConnection);

  connect(this, SIGNAL(logged(QString)), SLOT(log(QString)), Qt::QueuedConnection);
  connect(this, SIGNAL(warned(QString)), SLOT(warn(QString)), Qt::QueuedConnection);

  // Annotation graphics view
  connect(this, SIGNAL(addAnnotationsRequested(AnnotationList)),
          annotationView_, SLOT(addAnnotations(AnnotationList)),
          Qt::QueuedConnection);
  connect(this, SIGNAL(addAndShowAnnotationRequested(Annotation)),
          annotationView_, SLOT(addAndShowAnnotation(Annotation)),
          Qt::QueuedConnection);
  connect(this, SIGNAL(setAnnotationsRequested(AnnotationList)),
          annotationView_, SLOT(setAnnotations(AnnotationList)),
          Qt::QueuedConnection);
  connect(this, SIGNAL(appendAnnotationsRequested(AnnotationList)),
          annotationView_, SLOT(appendAnnotations(AnnotationList)),
          Qt::QueuedConnection);
  connect(this, SIGNAL(showAnnotationRequested(Annotation)),
          annotationView_, SLOT(showAnnotation(Annotation)),
          Qt::QueuedConnection);
  connect(this, SIGNAL(showAnnotationOnceRequested(Annotation)),
          annotationView_, SLOT(showAnnotationOnce(Annotation)),
          Qt::QueuedConnection);

  // Tokens
  connect(tokenView_, SIGNAL(aliasSubmitted(Alias)), SLOT(submitAlias(Alias)));
  connect(tokenView_, SIGNAL(tokenBlessedWithId(qint64)), SLOT(blessTokenWithId(qint64)));
  connect(tokenView_, SIGNAL(tokenCursedWithId(qint64)), SLOT(curseTokenWithId(qint64)));

  // Cast
  connect(annotationView_, SIGNAL(userBlessedWithId(qint64)), SLOT(blessUserWithId(qint64)));
  connect(annotationView_, SIGNAL(userCursedWithId(qint64)), SLOT(curseUserWithId(qint64)));
  connect(annotationView_, SIGNAL(userBlockedWithId(qint64)), SLOT(blockUserWithId(qint64)));

  connect(annotationView_, SIGNAL(userBlockedWithAlias(QString)), annotationFilter_, SLOT(addBlockedUserAlias(QString)));

  connect(annotationView_, SIGNAL(annotationBlessedWithId(qint64)), SLOT(blessAnnotationWithId(qint64)));
  connect(annotationView_, SIGNAL(annotationCursedWithId(qint64)), SLOT(curseAnnotationWithId(qint64)));
  connect(annotationView_, SIGNAL(annotationBlockedWithId(qint64)), SLOT(blockAnnotationWithId(qint64)));

  connect(annotationView_, SIGNAL(annotationBlockedWithText(QString)), annotationFilter_, SLOT(addBlockedText(QString)));

  connect(annotationBrowser_, SIGNAL(userBlessedWithId(qint64)), SLOT(blessUserWithId(qint64)));
  connect(annotationBrowser_, SIGNAL(userCursedWithId(qint64)), SLOT(curseUserWithId(qint64)));
  connect(annotationBrowser_, SIGNAL(userBlockedWithId(qint64)), SLOT(blockUserWithId(qint64)));

  connect(annotationBrowser_, SIGNAL(userBlockedWithAlias(QString)), annotationFilter_, SLOT(addBlockedUserAlias(QString)));

  connect(annotationBrowser_, SIGNAL(annotationBlessedWithId(qint64)), SLOT(blessAnnotationWithId(qint64)));
  connect(annotationBrowser_, SIGNAL(annotationCursedWithId(qint64)), SLOT(curseAnnotationWithId(qint64)));
  connect(annotationBrowser_, SIGNAL(annotationBlockedWithId(qint64)), SLOT(blockAnnotationWithId(qint64)));

  connect(annotationBrowser_, SIGNAL(annotationBlockedWithText(QString)), annotationFilter_, SLOT(addBlockedText(QString)));

  // Data manager
  connect(dataManager_, SIGNAL(tokenChanged(Token)), tokenView_, SLOT(setToken(Token)));
  connect(dataManager_, SIGNAL(aliasesChanged(AliasList)), tokenView_, SLOT(setAliases(AliasList)));
  connect(dataManager_, SIGNAL(aliasesChanged(AliasList)), SLOT(openAnnotationUrlFromAliases(AliasList)));
  //connect(tokenView_, SIGNAL(aliasSubmitted(Alias)), dataManager_, SLOT(addAlias(Alias)));

  connect(dataManager_, SIGNAL(aliasRemovedWithId(qint64)), dataServer_, SLOT(deleteAliasWithId(qint64)));
  connect(dataManager_, SIGNAL(annotationRemovedWithId(qint64)), dataServer_, SLOT(deleteAnnotationWithId(qint64)));
  connect(dataManager_, SIGNAL(annotationTextUpdatedWithId(QString,qint64)), dataServer_, SLOT(updateAnnotationTextWithId(QString,qint64)));

  connect(annotationView_, SIGNAL(annotationAdded(Annotation)), dataManager_, SLOT(addAnnotation(Annotation)));
  connect(annotationView_, SIGNAL(annotationsRemoved()), dataManager_, SLOT(removeAnnotations()));

  connect(annotationView_, SIGNAL(annotationsRemoved()), SLOT(clearAnnotationUrls()));

  connect(dataManager_, SIGNAL(annotationsChanged(AnnotationList)), annotationBrowser_, SLOT(setAnnotations(AnnotationList)));
  connect(dataManager_, SIGNAL(annotationAdded(Annotation)), annotationBrowser_, SLOT(addAnnotation(Annotation)));

  connect(tokenView_, SIGNAL(aliasDeletedWithId(qint64)), dataManager_, SLOT(removeAliasWithId(qint64)));

  connect(annotationBrowser_, SIGNAL(annotationDeletedWithId(qint64)), dataManager_, SLOT(removeAnnotationWithId(qint64)));
  connect(annotationBrowser_, SIGNAL(annotationDeletedWithId(qint64)), annotationView_, SLOT(removeAnnotationWithId(qint64)));

  connect(annotationView_, SIGNAL(annotationDeletedWithId(qint64)), dataManager_, SLOT(removeAnnotationWithId(qint64)));
  connect(annotationView_, SIGNAL(annotationDeletedWithId(qint64)), annotationBrowser_, SLOT(removeAnnotationWithId(qint64)));

  connect(annotationBrowser_, SIGNAL(annotationTextUpdatedWithId(QString,qint64)), annotationView_, SLOT(updateAnnotationTextWithId(QString,qint64)));
  connect(annotationView_, SIGNAL(annotationTextUpdatedWithId(QString,qint64)), annotationBrowser_, SLOT(updateAnnotationTextWithId(QString,qint64)));
  connect(annotationBrowser_, SIGNAL(annotationTextUpdatedWithId(QString,qint64)), dataManager_, SLOT(updateAnnotationTextWithId(QString,qint64)));
  connect(annotationView_, SIGNAL(annotationTextUpdatedWithId(QString,qint64)), dataManager_, SLOT(updateAnnotationTextWithId(QString,qint64)));

  // Forward drag/drop event
  connect(tokenView_, SIGNAL(dragEnterEventReceived(QDragEnterEvent*)), SLOT(dragEnterEvent(QDragEnterEvent*)));
  connect(tokenView_, SIGNAL(dragLeaveEventReceived(QDragLeaveEvent*)), SLOT(dragLeaveEvent(QDragLeaveEvent*)));
  connect(tokenView_, SIGNAL(dragMoveEventReceived(QDragMoveEvent*)), SLOT(dragMoveEvent(QDragMoveEvent*)));
  connect(tokenView_, SIGNAL(dropEventReceived(QDropEvent*)), SLOT(dropEvent(QDropEvent*)));

#ifdef USE_MODE_SIGNAL
  //connect(signalView_->tokenView(), SIGNAL(aliasSubmitted(Alias)), SLOT(submitAlias(Alias)));

  connect(signalView_, SIGNAL(dragEnterEventReceived(QDragEnterEvent*)), SLOT(dragEnterEvent(QDragEnterEvent*)));
  connect(signalView_, SIGNAL(dragLeaveEventReceived(QDragLeaveEvent*)), SLOT(dragLeaveEvent(QDragLeaveEvent*)));
  connect(signalView_, SIGNAL(dragMoveEventReceived(QDragMoveEvent*)), SLOT(dragMoveEvent(QDragMoveEvent*)));
  connect(signalView_, SIGNAL(dropEventReceived(QDropEvent*)), SLOT(dropEvent(QDropEvent*)));
#endif // USE_MODE_SIGNAL


  connect(annotationBrowser_, SIGNAL(dragEnterEventReceived(QDragEnterEvent*)), SLOT(dragEnterEvent(QDragEnterEvent*)));
  connect(annotationBrowser_, SIGNAL(dragLeaveEventReceived(QDragLeaveEvent*)), SLOT(dragLeaveEvent(QDragLeaveEvent*)));
  connect(annotationBrowser_, SIGNAL(dragMoveEventReceived(QDragMoveEvent*)), SLOT(dragMoveEvent(QDragMoveEvent*)));
  connect(annotationBrowser_, SIGNAL(dropEventReceived(QDropEvent*)), SLOT(dropEvent(QDropEvent*)));

  connect(miniPlayer_, SIGNAL(dragEnterEventReceived(QDragEnterEvent*)), SLOT(dragEnterEvent(QDragEnterEvent*)));
  connect(miniPlayer_, SIGNAL(dragLeaveEventReceived(QDragLeaveEvent*)), SLOT(dragLeaveEvent(QDragLeaveEvent*)));
  connect(miniPlayer_, SIGNAL(dragMoveEventReceived(QDragMoveEvent*)), SLOT(dragMoveEvent(QDragMoveEvent*)));
  connect(miniPlayer_, SIGNAL(dropEventReceived(QDropEvent*)), SLOT(dropEvent(QDropEvent*)));

  //connect(miniPlayer_, SIGNAL(togglePlayModeRequested()), hub_, SLOT(toggleFullScreenWindowMode()));
  //connect(miniPlayer_, SIGNAL(toggleMiniModeRequested()), hub_, SLOT(toggleMiniPlayerMode()));

  // Live mode
  //connect(player_, SIGNAL(mediaChanged()), this, SLOT(stopLiveMode()));
  //connect(player_, SIGNAL(mediaClosed()), this, SLOT(stopLiveMode()));
  //connect(player_, SIGNAL(paused()), this, SLOT(stopLiveMode()));
  //connect(player_, SIGNAL(stopped()), this, SLOT(stopLiveMode()));
  //connect(this, SIGNAL(seeked()), this, SLOT(stopLiveMode()));

  // Sync mode
  connect(player_, SIGNAL(mediaChanged()), hub_, SLOT(stopSyncPlayMode()));
  connect(player_, SIGNAL(mediaClosed()), hub_, SLOT(stopSyncPlayMode()));
  connect(player_, SIGNAL(paused()), hub_, SLOT(stopSyncPlayMode()));
  connect(player_, SIGNAL(stopped()), hub_, SLOT(stopSyncPlayMode()));
  connect(this, SIGNAL(seeked()), hub_, SLOT(stopSyncPlayMode()));

  // Tracked window
  connect(annotationView_, SIGNAL(trackedWindowChanged(WId)), embeddedPlayer_, SLOT(setContainerWindow(WId)));

#ifdef Q_WS_WIN
  connect(annotationView_, SIGNAL(trackedWindowChanged(WId)), SLOT(setEmbeddedWindow(WId)));
#endif // Q_WS_WIN

  //connect(annotationView_, SIGNAL(trackedWindowDestroyed()), hub_, SLOT(setNormalPlayerMode()));
  //connect(embeddedPlayer_, SIGNAL(trackedWindowDestroyed()), hub_, SLOT(setNormalPlayerMode()));
  connect(annotationView_, SIGNAL(trackedWindowChanged(WId)), SLOT(invalidatePlayerMode()));

  connect(annotationView_, SIGNAL(fullScreenModeChanged(bool)), embeddedPlayer_, SLOT(setFullScreenMode(bool)));

  // Synchronize text edit
  //#define SYNC(_playerui1, _playerui2)
  //  connect(_playerui1->lineEdit(), SIGNAL(textChanged(QString)), _playerui2->lineEdit(), SLOT(setText(QString)));
  //  connect(_playerui2->lineEdit(), SIGNAL(textChanged(QString)), _playerui1->lineEdit(), SLOT(setText(QString)));
  //SYNC(mainPlayer_, miniPlayer_)
  //SYNC(mainPlayer_, embeddedPlayer_)
  //SYNC(miniPlayer_, embeddedPlayer_)
  //#undef SYNC

  //connect(server_, SIGNAL(annotationsReceived(AnnotationList)),
  //        annotationView_, SLOT(addAnnotations(AnnotationList)));
  //connect(server_, SIGNAL(tokensReceived(TokenList)),
  //        this, SLOT(processTokens(TokenList)));

#ifdef Q_WS_MAC
  connect(player_, SIGNAL(playing()), SLOT(showVideoViewIfAvailable()));
  connect(player_, SIGNAL(stopped()), videoView_, SLOT(hideView()));
#endif // Q_WS_MAC

#ifdef Q_WS_WIN
  if (!QtWin::isWindowsVistaOrLater())
#endif // Q_WS_WIN
  {
    connect(player_, SIGNAL(playing()), SLOT(disableWindowTransparency()));
    connect(player_, SIGNAL(stopped()), SLOT(enableWindowTransparency()));
  }

  // Agents:
#ifdef USE_MODULE_CLIENTAGENT
  connect(client_, SIGNAL(chatReceived(QString)), SLOT(respond(QString)));
#endif // USE_MODULE_CLIENTAGENT

  // Language:
  connect(TranslatorManager::globalInstance(), SIGNAL(languageChanged()), SLOT(invalidateAppLanguage()));

  // Translator:
  connect(translator_, SIGNAL(translated(QString)), SLOT(showTextAsSubtitle(QString)));

  // Logger:

  connect(cache_, SIGNAL(cleared()), logger_, SLOT(logCacheCleared()));

  connect(server_, SIGNAL(loginRequested(QString)), logger_, SLOT(logLoginRequested(QString)));
  connect(server_, SIGNAL(loginSucceeded(QString)), logger_, SLOT(logLoginSucceeded(QString)));
  connect(server_, SIGNAL(loginFailed(QString)), logger_, SLOT(logLoginFailed(QString)));
  connect(server_, SIGNAL(logoutRequested()), logger_, SLOT(logLogoutRequested()));
  connect(server_, SIGNAL(logoutFinished()), logger_, SLOT(logLogoutFinished()));
  connect(server_, SIGNAL(unknownError()), logger_, SLOT(logServerAgentUnknownError()));
  connect(server_, SIGNAL(connectionError()), logger_, SLOT(logServerAgentConnectionError()));
  connect(server_, SIGNAL(serverError()), logger_, SLOT(logServerAgentServerError()));
  connect(server_, SIGNAL(error404()), logger_, SLOT(logServerAgentError404()));

  connect(translator_, SIGNAL(networkError(QString)), logger_, SLOT(logTranslatorNetworkError(QString)));

  connect(annotationView_, SIGNAL(trackedWindowDestroyed()), logger_, SLOT(logTrackedWindowDestroyed()));

#ifdef USE_MODULE_CLIENTAGENT
  connect(client_, SIGNAL(authorized()), logger_, SLOT(logClientAgentAuthorized()));
  connect(client_, SIGNAL(deauthorized()), logger_, SLOT(logClientAgentDeauthorized()));
  connect(client_, SIGNAL(authorizationError()), logger_, SLOT(logClientAgentAuthorizationError()));
#endif // USE_MODULE_CLIENTAGENT

  connect(annotationView_, SIGNAL(annotationPosChanged(qint64)), annotationBrowser_, SLOT(setAnnotationPos(qint64)));

#ifdef USE_MODE_SIGNAL
  connect(signalView_, SIGNAL(hookSelected(ulong,ProcessInfo)), SLOT(openProcessHook(ulong,ProcessInfo)));
  connect(recentMessageView_, SIGNAL(hookSelected(ulong)), SLOT(openProcessHook(ulong)));
  connect(recentMessageView_, SIGNAL(hookSelected(ulong)), recentMessageView_, SLOT(hide()));
  connect(messageHandler_, SIGNAL(messageReceivedWithId(qint64)), annotationView_, SLOT(showAnnotationsAtPos(qint64)));
  connect(messageHandler_, SIGNAL(messageReceivedWithText(QString)), SLOT(translate(QString)));

  connect(messageHandler_, SIGNAL(messageReceivedWithId(qint64)), annotationBrowser_, SLOT(setAnnotationPos(qint64)));

  // Ensure backlogDialog enabled in signal mode.
  connect(signalView_, SIGNAL(hookSelected(ulong,ProcessInfo)), SLOT(backlogDialog()));
#endif // USE_MODE_SIGNAL
  //connect(player_, SIGNAL(opening()), SLOT(backlogDialog()));

  // MRL resolver
  connect(mrlResolver_, SIGNAL(mediaResolved(MediaInfo,QNetworkCookieJar*)), SLOT(openRemoteMedia(MediaInfo,QNetworkCookieJar*)));
  connect(mrlResolver_, SIGNAL(subtitleResolved(QString)), SLOT(importAnnotationsFromUrl(QString)));
  connect(mrlResolver_, SIGNAL(messageReceived(QString)), SLOT(log(QString)));
  connect(mrlResolver_, SIGNAL(errorReceived(QString)), SLOT(warn(QString)));

  // Annotation codec
  {
    AnnotationCodecManager *acm = AnnotationCodecManager::globalInstance();
    connect(acm, SIGNAL(messageReceived(QString)), SLOT(log(QString)));
    connect(acm, SIGNAL(errorReceived(QString)), SLOT(warn(QString)));
    connect(acm, SIGNAL(fetched(AnnotationList,QString)), SLOT(addRemoteAnnotations(AnnotationList,QString)));
  }

  // Streaming service
  //{
  //  StreamService *ss = StreamService::globalInstance();
  //  connect(ss, SIGNAL(messageReceived(QString)), SLOT(log(QString)));
  //  connect(ss, SIGNAL(errorReceived(QString)), SLOT(warn(QString)));
  //  connect(ss, SIGNAL(streamReady(QString)), SLOT(openStreamUrl(QString)));
  //}

  // - Close
  //connect(this, SIGNAL(closing()), db_, SLOT(dispose));
}

void
MainWindow::parseArguments(const QStringList &args)
{
  if (args.size() <= 1)
    return;

  playlist_ = args;
  openMrl(args.back());
}

bool
MainWindow::isValid() const
{ return player_ && player_->isValid();}

void
MainWindow::createActions()
{
#define MAKE_ACTION(_action, _styleid, _receiver, _slot) { \
    _action = new QAction(QIcon(RC_IMAGE_##_styleid), TR(T_MENUTEXT_##_styleid), this); \
    _action->setToolTip(TR(T_TOOLTIP_##_styleid)); \
  } connect(_action, SIGNAL(triggered()), _receiver, _slot);
#define MAKE_TOGGLE(_action, _styleid, _receiver, _slot) { \
    _action = new QAction(QIcon(RC_IMAGE_##_styleid), TR(T_MENUTEXT_##_styleid), this); \
    _action->setToolTip(TR(T_TOOLTIP_##_styleid)); \
    _action->setCheckable(true); \
  } connect(_action, SIGNAL(triggered(bool)), _receiver, _slot);

  MAKE_ACTION(openAct_,         OPEN,           hub_,           SLOT(open()))
  MAKE_ACTION(openFileAct_,     OPENFILE,       this,           SLOT(openFile()))
  MAKE_ACTION(openUrlAct_,      OPENURL,        this,           SLOT(openUrl()))
  MAKE_ACTION(openAnnotationUrlAct_, OPENANNOTATIONURL, this,   SLOT(openAnnotationUrl()))
  MAKE_ACTION(openDeviceAct_,   OPENDEVICE,     this,           SLOT(openDevice()))
  MAKE_ACTION(downloadCurrentUrlAct_,DOWNLOADCURRENT, this,      SLOT(downloadCurrentUrl()))
  MAKE_ACTION(openInWebBrowserAct_,OPENINWEBBROWSER, this,      SLOT(openInWebBrowser()))
  MAKE_ACTION(openVideoDeviceAct_, OPENVIDEODEVICE,  this,      SLOT(openVideoDevice()))
  MAKE_ACTION(openAudioDeviceAct_, OPENAUDIODEVICE,  this,      SLOT(openAudioDevice()))
  MAKE_ACTION(openSubtitleAct_, OPENSUBTITLE,   this,           SLOT(openSubtitle()))
  MAKE_ACTION(playAct_,         PLAY,           hub_,           SLOT(play()))
  MAKE_ACTION(pauseAct_,        PAUSE,          hub_,           SLOT(pause()))
  MAKE_ACTION(previousAct_,     PREVIOUS,       this,           SLOT(previous()))
  MAKE_ACTION(nextAct_,         NEXT,           this,           SLOT(next()))
  MAKE_ACTION(nextFrameAct_,    NEXTFRAME,      player_,        SLOT(nextFrame()))
  MAKE_ACTION(stopAct_,         STOP,           hub_,           SLOT(stop()))
  MAKE_ACTION(replayAct_,       REPLAY,         this,           SLOT(replay()))
  MAKE_ACTION(snapshotAct_,     SNAPSHOT,       this,           SLOT(snapshot()))
  MAKE_ACTION(checkInternetConnectionAct_,     CHECKINTERNET,   this, SLOT(checkInternetConnection()))
  MAKE_ACTION(deleteCachesAct_, DELETECACHE,   this, SLOT(deleteCaches()))
  MAKE_TOGGLE(toggleFullScreenModeAct_, FULLSCREEN, hub_,       SLOT(setFullScreenWindowMode(bool)))
  MAKE_TOGGLE(toggleMenuBarVisibleAct_, SHOWMENUBAR, menuBar(), SLOT(setVisible(bool)))
  MAKE_TOGGLE(toggleAnnotationCountDialogVisibleAct_, ANNOTATIONLIMIT, this, SLOT(setAnnotationCountDialogVisible(bool)))
  MAKE_TOGGLE(toggleEmbeddedModeAct_, EMBED,    hub_,           SLOT(setEmbeddedPlayerMode(bool)))
  MAKE_TOGGLE(toggleMiniModeAct_, MINI,         hub_,           SLOT(setMiniPlayerMode(bool)))
  MAKE_TOGGLE(toggleLiveModeAct_, LIVE,         hub_,           SLOT(setLiveTokenMode(bool)))
  MAKE_TOGGLE(toggleSyncModeAct_, SYNC,         hub_,           SLOT(setSyncPlayMode(bool)))
  MAKE_TOGGLE(toggleAnnotationVisibleAct_, SHOWANNOT, annotationView_, SLOT(setVisible(bool)))
  MAKE_TOGGLE(toggleSubtitleVisibleAct_, SHOWSUBTITLE, player_, SLOT(setSubtitleVisible(bool)))
  MAKE_TOGGLE(toggleSubtitleAnnotationVisibleAct_, SUBANNOT, annotationView_, SLOT(setSubtitleVisible(bool)))
  MAKE_TOGGLE(toggleNonSubtitleAnnotationVisibleAct_, NONSUBANNOT, annotationView_, SLOT(setNonSubtitleVisible(bool)))
  MAKE_TOGGLE(toggleWindowOnTopAct_, WINDOWSTAYSONTOP, this, SLOT(setWindowOnTop(bool)))
  MAKE_TOGGLE(toggleUserAnonymousAct_,  ANONYMOUS,       this,         SLOT(setUserAnonymous(bool)))
  MAKE_TOGGLE(toggleUserViewVisibleAct_, USER,          this,         SLOT(setUserViewVisible(bool)))
  MAKE_TOGGLE(toggleBlacklistViewVisibleAct_, BLACKLIST,   this,    SLOT(setBlacklistViewVisible(bool)))
  MAKE_TOGGLE(toggleAnnotationFilterEnabledAct_, ENABLEBLACKLIST,   annotationFilter_,    SLOT(setEnabled(bool)))
  MAKE_TOGGLE(toggleBacklogDialogVisibleAct_, BACKLOG,   this,    SLOT(setBacklogDialogVisible(bool)))
  MAKE_TOGGLE(toggleDownloadDialogVisibleAct_, DOWNLOAD,   this,    SLOT(setDownloadDialogVisible(bool)))
  MAKE_TOGGLE(toggleConsoleDialogVisibleAct_, CONSOLE,   this,    SLOT(setConsoleDialogVisible(bool)))
  MAKE_TOGGLE(toggleLoginDialogVisibleAct_, LOGINDIALOG, this,         SLOT(setLoginDialogVisible(bool)))
  MAKE_TOGGLE(toggleWindowPickDialogVisibleAct_,  WINDOWPICKDIALOG,  this,         SLOT(setWindowPickDialogVisible(bool)))
  MAKE_TOGGLE(toggleProcessPickDialogVisibleAct_, PROCESSPICKDIALOG, this,         SLOT(setProcessPickDialogVisible(bool)))
  MAKE_TOGGLE(toggleSeekDialogVisibleAct_,  SEEKDIALOG,  this,         SLOT(setSeekDialogVisible(bool)))
  MAKE_TOGGLE(toggleLiveDialogVisibleAct_,  LIVEDIALOG,  this,         SLOT(setLiveDialogVisible(bool)))
  MAKE_TOGGLE(toggleSyncDialogVisibleAct_,  SYNCDIALOG,  this,         SLOT(setSyncDialogVisible(bool)))
  MAKE_TOGGLE(toggleSiteAccountViewVisibleAct_, SITEACCOUNT, this, SLOT(setSiteAccountViewVisible(bool)))
  MAKE_TOGGLE(toggleAnnotationBrowserVisibleAct_, ANNOTATIONBROWSER, annotationBrowser_, SLOT(setVisible(bool)))
  MAKE_TOGGLE(toggleAnnotationEditorVisibleAct_, ANNOTATIONEDITOR, this, SLOT(setAnnotationEditorVisible(bool)))
  MAKE_TOGGLE(toggleTokenViewVisibleAct_, TOKENVIEW, tokenView_, SLOT(setVisible(bool)))
  //MAKE_TOGGLE(toggleCommentViewVisibleAct_, COMMENTVIEW, this, SLOT(setCommentViewVisible(bool)))
  MAKE_ACTION(openHomePageAct_, CLOUDVIEW, this, SLOT(openHomePage()))
  MAKE_TOGGLE(toggleTranslateAct_, TRANSLATE,   this,           SLOT(setTranslateEnabled(bool)))
  MAKE_TOGGLE(toggleSubtitleOnTopAct_, SUBTITLEONTOP,   this,  SLOT(setSubtitleOnTop(bool)))
  MAKE_TOGGLE(toggleEmbeddedPlayerOnTopAct_, EMBEDONTOP,   embeddedPlayer_,  SLOT(setOnTop(bool)))
  MAKE_ACTION(loginAct_,        LOGIN,          this,           SLOT(showLoginDialog()))
  MAKE_ACTION(logoutAct_,       LOGOUT,         this,           SLOT(logout()))
  MAKE_ACTION(clearCacheAct_,   CLEARCACHE,     cache_,         SLOT(clear()))
  MAKE_ACTION(connectAct_,      CONNECT,        server_,        SLOT(updateConnected()))
  MAKE_ACTION(disconnectAct_,   DISCONNECT,     server_,        SLOT(disconnect()))
  MAKE_TOGGLE(setSubtitleColorToDefaultAct_, DEFAULTCOLOR, this, SLOT(setSubtitleColorToDefault()))
  MAKE_TOGGLE(setSubtitleColorToWhiteAct_, WHITECOLOR, this, SLOT(setSubtitleColorToWhite()))
  MAKE_TOGGLE(setSubtitleColorToCyanAct_, CYANCOLOR, this, SLOT(setSubtitleColorToCyan()))
  MAKE_TOGGLE(setSubtitleColorToBlueAct_, BLUECOLOR, this, SLOT(setSubtitleColorToBlue()))
  MAKE_TOGGLE(setSubtitleColorToOrangeAct_, ORANGECOLOR, this, SLOT(setSubtitleColorToOrange()))
  MAKE_TOGGLE(setSubtitleColorToPurpleAct_, PURPLECOLOR, this, SLOT(setSubtitleColorToPurple()))
  MAKE_TOGGLE(setSubtitleColorToBlackAct_, BLACKCOLOR, this, SLOT(setSubtitleColorToBlack()))
  MAKE_TOGGLE(setSubtitleColorToRedAct_, REDCOLOR, this, SLOT(setSubtitleColorToRed()))
  MAKE_TOGGLE(setAnnotationEffectToDefaultAct_, AUTO, this, SLOT(setAnnotationEffectToDefault()))
  MAKE_TOGGLE(setAnnotationEffectToTransparentAct_, TRANSPARENT, this, SLOT(setAnnotationEffectToTransparent()))
  MAKE_TOGGLE(setAnnotationEffectToShadowAct_, SHADOW, this, SLOT(setAnnotationEffectToShadow()))
  MAKE_TOGGLE(setAnnotationEffectToBlurAct_, BLUR, this, SLOT(setAnnotationEffectToBlur()))
#ifdef USE_MODE_SIGNAL
  MAKE_TOGGLE(toggleSignalViewVisibleAct_, SIGNALVIEW, this, SLOT(setSignalViewVisible(bool)))
  MAKE_TOGGLE(toggleRecentMessageViewVisibleAct_, RECENTMESSAGES, this, SLOT(setRecentMessageViewVisible(bool)))
#endif // USE_MODE_SIGNAL

  MAKE_ACTION(forward5sAct_,    FORWARD5S,      this,   SLOT(forward5s()))
  MAKE_ACTION(backward5sAct_,   BACKWARD5S,     this,   SLOT(backward5s()))
  MAKE_ACTION(forward10sAct_,   FORWARD10S,     this,   SLOT(forward10s()))
  MAKE_ACTION(backward10sAct_,  BACKWARD10S,    this,   SLOT(backward10s()))
  MAKE_ACTION(forward30sAct_,   FORWARD30S,     this,   SLOT(forward30s()))
  MAKE_ACTION(backward30sAct_,  BACKWARD30S,    this,   SLOT(backward30s()))
  MAKE_ACTION(forward90sAct_,   FORWARD60S,     this,   SLOT(forward60s()))
  MAKE_ACTION(backward90sAct_,  BACKWARD60S,    this,   SLOT(backward60s()))
  MAKE_ACTION(forward90sAct_,   FORWARD90S,     this,   SLOT(forward90s()))
  MAKE_ACTION(backward90sAct_,  BACKWARD90S,    this,   SLOT(backward90s()))
  MAKE_ACTION(forward1mAct_,    FORWARD1M,      this,   SLOT(forward1m()))
  MAKE_ACTION(backward1mAct_,   BACKWARD1M,     this,   SLOT(backward1m()))
  MAKE_ACTION(forward5mAct_,    FORWARD5M,      this,   SLOT(forward5m()))
  MAKE_ACTION(backward5mAct_,   BACKWARD5M,     this,   SLOT(backward5m()))
  MAKE_ACTION(forward10mAct_,   FORWARD10M,     this,   SLOT(forward10m()))
  MAKE_ACTION(backward10mAct_,  BACKWARD10M,    this,   SLOT(backward10m()))

  MAKE_ACTION(clearRecentAct_,  CLEARRECENT,    this,   SLOT(clearRecent()))
  MAKE_ACTION(clearPlaylistAct_,CLEARPLAYLIST,  this,   SLOT(clearPlaylist()))
  MAKE_ACTION(nextPlaylistItemAct_,NEXT,        this,   SLOT(openNextPlaylistItem()))
  MAKE_ACTION(previousPlaylistItemAct_,PREVIOUS,this,   SLOT(openPreviousPlaylistItem()))

  MAKE_ACTION(previousSectionAct_, PREVIOUSSECTION, player_,  SLOT(setPreviousTitle()))
  MAKE_ACTION(nextSectionAct_,     NEXTSECTION,    player_,   SLOT(setNextTitle()))

  MAKE_ACTION(previousFileAct_, PREVIOUS, this,  SLOT(openPreviousFile()))
  MAKE_ACTION(nextFileAct_,     NEXT,     this,  SLOT(openNextFile()))

  MAKE_TOGGLE(setAppLanguageToEnglishAct_,  ENGLISH, this, SLOT(setAppLanguageToEnglish()))
  MAKE_TOGGLE(setAppLanguageToJapaneseAct_, JAPANESE,this, SLOT(setAppLanguageToJapanese()))
  MAKE_TOGGLE(setAppLanguageToTraditionalChineseAct_,TRADITIONALCHINESE, this, SLOT(setAppLanguageToTraditionalChinese()))
  MAKE_TOGGLE(setAppLanguageToSimplifiedChineseAct_, SIMPLIFIEDCHINESE, this, SLOT(setAppLanguageToSimplifiedChinese()))

  MAKE_TOGGLE(setUserLanguageToEnglishAct_,  ENGLISH, this, SLOT(setUserLanguageToEnglish()))
  MAKE_TOGGLE(setUserLanguageToJapaneseAct_, JAPANESE,this, SLOT(setUserLanguageToJapanese()))
  MAKE_TOGGLE(setUserLanguageToChineseAct_, CHINESE, this, SLOT(setUserLanguageToChinese()))
  MAKE_TOGGLE(setUserLanguageToKoreanAct_, KOREAN, this, SLOT(setUserLanguageToKorean()))
  MAKE_TOGGLE(setUserLanguageToUnknownAct_, UNKNOWNLANGUAGE, this, SLOT(setUserLanguageToUnknown()))

  MAKE_TOGGLE(toggleAnnotationLanguageToEnglishAct_,  ENGLISH, this, SLOT(invalidateAnnotationLanguages()))
  MAKE_TOGGLE(toggleAnnotationLanguageToJapaneseAct_, JAPANESE,this, SLOT(invalidateAnnotationLanguages()))
  MAKE_TOGGLE(toggleAnnotationLanguageToChineseAct_, CHINESE, this, SLOT(invalidateAnnotationLanguages()))
  MAKE_TOGGLE(toggleAnnotationLanguageToKoreanAct_, KOREAN, this, SLOT(invalidateAnnotationLanguages()))
  MAKE_TOGGLE(toggleAnnotationLanguageToUnknownAct_, UNKNOWNLANGUAGE, this, SLOT(invalidateAnnotationLanguages()))
  MAKE_TOGGLE(toggleAnnotationLanguageToAnyAct_, ANYLANGUAGE, this,SLOT(invalidateAnnotationLanguages()))

  MAKE_ACTION(showMaximizedAct_,        MAXIMIZE,       this,    SLOT(showMaximized()))
  MAKE_ACTION(showMinimizedAct_,        MINIMIZE,       this,    SLOT(showMinimizedAndPause()))
  MAKE_ACTION(showNormalAct_,           RESTORE,        this,    SLOT(showNormal()))

  toggleAutoPlayNextAct_ = new QAction(TR(T_MENUTEXT_AUTOPLAYNEXT), this); {
    toggleAutoPlayNextAct_->setToolTip(TR(T_TOOLTIP_AUTOPLAYNEXT));
    toggleAutoPlayNextAct_->setCheckable(true);
    toggleAutoPlayNextAct_->setChecked(Settings::globalInstance()->isAutoPlayNext());
  }

#ifdef Q_WS_WIN
  if (!QtWin::isWindowsVistaOrLater())
#endif // Q_WS_WIN
  {
    MAKE_TOGGLE(setThemeToDefaultAct_,    DEFAULTTHEME,   this,   SLOT(setThemeToDefault()))
    MAKE_TOGGLE(setThemeToRandomAct_,     RANDOMTHEME,    this,   SLOT(setThemeToRandom()))
    MAKE_TOGGLE(setThemeToDarkAct_,       DARKTHEME,      this,   SLOT(setThemeToDark()))
    MAKE_TOGGLE(setThemeToBlackAct_,      BLACKTHEME,     this,   SLOT(setThemeToBlack()))
    MAKE_TOGGLE(setThemeToBlueAct_,       BLUETHEME,      this,   SLOT(setThemeToBlue()))
    MAKE_TOGGLE(setThemeToBrownAct_,      BROWNTHEME,     this,   SLOT(setThemeToBrown()))
    MAKE_TOGGLE(setThemeToCyanAct_,       CYANTHEME,      this,   SLOT(setThemeToCyan()))
    MAKE_TOGGLE(setThemeToGrayAct_,       GRAYTHEME,      this,   SLOT(setThemeToGray()))
    MAKE_TOGGLE(setThemeToGreenAct_,      GREENTHEME,     this,   SLOT(setThemeToGreen()))
    MAKE_TOGGLE(setThemeToPinkAct_,       PINKTHEME,      this,   SLOT(setThemeToPink()))
    MAKE_TOGGLE(setThemeToPurpleAct_,     PURPLETHEME,    this,   SLOT(setThemeToPurple()))
    MAKE_TOGGLE(setThemeToRedAct_,        REDTHEME,       this,   SLOT(setThemeToRed()))
    MAKE_TOGGLE(setThemeToWhiteAct_,      WHITETHEME,     this,   SLOT(setThemeToWhite()))
    MAKE_TOGGLE(setThemeToYellowAct_,     YELLOWTHEME,    this,   SLOT(setThemeToYellow()))
  }

  MAKE_ACTION(aboutAct_,        ABOUT,  this,   SLOT(about()))
  MAKE_ACTION(helpAct_,         HELP,   this,   SLOT(help()))
  MAKE_ACTION(updateAct_,       UPDATE, this,   SLOT(update()))
  MAKE_ACTION(quitAct_,         QUIT,   this,   SLOT(close()))

#undef MAKE_ACTION

  // Shortcuts
  {
    toggleSeekDialogVisibleAct_->setShortcuts(QKeySequence::Find);
    QShortcut *f = new QShortcut(QKeySequence::Find, this);
    connect(f, SIGNAL(activated()), SLOT(showSeekDialog()));

    nextFrameAct_->setShortcuts(QKeySequence::FindNext);
    QShortcut *g = new QShortcut(QKeySequence::FindNext, this);
    connect(g, SIGNAL(activated()), SLOT(nextFrame()));

    openAct_->setShortcuts(QKeySequence::Open);
    openFileAct_->setShortcuts(QKeySequence::Open);
    QShortcut *o = new QShortcut(QKeySequence::Open, this);
    connect(o, SIGNAL(activated()), SLOT(open()));

    openUrlAct_->setShortcuts(QKeySequence::New);
    QShortcut *n = new QShortcut(QKeySequence::New, this);
    connect(n, SIGNAL(activated()), SLOT(openUrl()));

    openAnnotationUrlAct_->setShortcuts(QKeySequence::Italic);
    QShortcut *i = new QShortcut(QKeySequence::Italic, this);
    connect(i, SIGNAL(activated()), SLOT(openUrl()));

    snapshotAct_->setShortcut(QKeySequence::Save);
    QShortcut *s = new QShortcut(QKeySequence::Save, this);
    connect(s, SIGNAL(activated()), SLOT(snapshot()));

    toggleWindowOnTopAct_->setShortcuts(QKeySequence::AddTab);
    QShortcut *t = new QShortcut(QKeySequence::AddTab, this);
    connect(t, SIGNAL(activated()), SLOT(toggleWindowOnTop()));

    helpAct_->setShortcuts(QKeySequence::HelpContents);
    QShortcut *help = new QShortcut(QKeySequence::HelpContents, this);
    connect(help, SIGNAL(activated()), SLOT(help()));

    playAct_->setShortcuts(QKeySequence::Print);
    pauseAct_->setShortcuts(QKeySequence::Print);
    QShortcut *p = new QShortcut(QKeySequence::Print, this);
    connect(p, SIGNAL(activated()), SLOT(playPause()));

    quitAct_->setShortcuts(QKeySequence::Quit);

    QShortcut *c1 = new QShortcut(QKeySequence("CTRL+1"), this);
    connect(c1, SIGNAL(activated()), hub_, SLOT(toggleEmbeddedPlayerMode()));
    QShortcut *c2 = new QShortcut(QKeySequence("CTRL+2"), this);
    connect(c2, SIGNAL(activated()), hub_, SLOT(toggleMiniPlayerMode()));
    QShortcut *c3 = new QShortcut(QKeySequence("CTRL+3"), this);
    connect(c3, SIGNAL(activated()), hub_, SLOT(toggleFullScreenWindowMode()));

    QShortcut *f11 = new QShortcut(QKeySequence("F11"), this);
    connect(f11, SIGNAL(activated()), hub_, SLOT(toggleFullScreenWindowMode()));

    QShortcut *cf1 = new QShortcut(QKeySequence("CTRL+F1"), this);
    connect(cf1, SIGNAL(activated()), SLOT(toggleAnnotationEditorVisible()));
    QShortcut *cf2 = new QShortcut(QKeySequence("CTRL+F2"), this);
    connect(cf2, SIGNAL(activated()), SLOT(toggleAnnotationBrowserVisible()));
    QShortcut *cf3 = new QShortcut(QKeySequence("CTRL+F3"), this);
    connect(cf3, SIGNAL(activated()), SLOT(toggleTokenViewVisible()));
    QShortcut *cf4 = new QShortcut(QKeySequence("CTRL+F4"), this);
    connect(cf4, SIGNAL(activated()), SLOT(showBlacklistView()));
    QShortcut *cf5 = new QShortcut(QKeySequence("CTRL+F5"), this);
    connect(cf5, SIGNAL(activated()), SLOT(showDownloadDialog()));

    QShortcut *pp = new QShortcut(QKeySequence("CTRL+SHIFT+LEFT"), this);
    connect(pp, SIGNAL(activated()), SLOT(previous()));
    QShortcut *nn = new QShortcut(QKeySequence("CTRL+SHIFT+RIGHT"), this);
    connect(nn, SIGNAL(activated()), SLOT(next()));

    QShortcut *backward = new QShortcut(QKeySequence("CTRL+LEFT"), this);
    connect(backward, SIGNAL(activated()), SLOT(backward90s()));
    QShortcut *forward = new QShortcut(QKeySequence("CTRL+RIGHT"), this);
    connect(forward, SIGNAL(activated()), SLOT(forward90s()));

    QShortcut *esc = new QShortcut(QKeySequence("Esc"), this);
    connect(esc, SIGNAL(activated()), SLOT(showMinimizedAndPause()));

    QShortcut *up = new QShortcut(QKeySequence("CTRL+UP"), this);
    connect(up, SIGNAL(activated()), SLOT(volumeUp()));
    QShortcut *down = new QShortcut(QKeySequence("CTRL+DOWN"), this);
    connect(down, SIGNAL(activated()), SLOT(volumeDown()));
    QShortcut *csup = new QShortcut(QKeySequence("CTRL+SHIFTUP"), this);
    connect(csup, SIGNAL(activated()), SLOT(volumeUp()));
    QShortcut *csdown = new QShortcut(QKeySequence("CTRL+SHIFT+DOWN"), this);
    connect(csdown, SIGNAL(activated()), SLOT(volumeDown()));
  }
}

void
MainWindow::createMenus()
{
  // Context menu
  contextMenu_ = new QMenu(TR(T_TITLE_PROGRAM), this); {
    UiStyle::globalInstance()->setContextMenuStyle(contextMenu_, true); // persistent = true
    embeddedPlayer_->setMenu(contextMenu_);
    mainPlayer_->setMenu(contextMenu_);
    miniPlayer_->setMenu(contextMenu_);
  }

  userMenu_ = new QMenu(TR(T_TITLE_PROGRAM), this); {
    UiStyle::globalInstance()->setContextMenuStyle(userMenu_, true); // persistent = true
    PlayerUi *players[] = { mainPlayer_, miniPlayer_, embeddedPlayer_ };
    BOOST_FOREACH (PlayerUi *p, players) {
      p->userButton()->setMenu(userMenu_);
      connect(p, SIGNAL(invalidateUserMenuRequested()), SLOT(invalidateUserMenu()));
    }
  }

  helpContextMenu_ = new QMenu(this); {
    UiStyle::globalInstance()->setContextMenuStyle(helpContextMenu_, true); // persistent = true
    helpContextMenu_->setTitle(TR(T_MENUTEXT_HELP) + " ...");
    helpContextMenu_->setToolTip(TR(T_TOOLTIP_HELP));

    helpContextMenu_->addAction(helpAct_);
    helpContextMenu_->addAction(openHomePageAct_);
    helpContextMenu_->addAction(updateAct_);

    toggleConsoleDialogVisibleAct_->setChecked(consoleDialog_ && consoleDialog_->isVisible());
    helpContextMenu_->addAction(toggleConsoleDialogVisibleAct_);

    helpContextMenu_->addAction(aboutAct_);
  }

  annotationEffectMenu_ = new QMenu(this); {
    UiStyle::globalInstance()->setContextMenuStyle(annotationEffectMenu_, true); // persistent = true

    annotationEffectMenu_->setTitle(TR(T_ANNOTATIONEFFECT) + " ...");
    annotationEffectMenu_->setToolTip(TR(T_ANNOTATIONEFFECT));

    annotationEffectMenu_->addAction(setAnnotationEffectToDefaultAct_);
    annotationEffectMenu_->addSeparator();
    annotationEffectMenu_->addAction(setAnnotationEffectToTransparentAct_);
    annotationEffectMenu_->addAction(setAnnotationEffectToShadowAct_);
    annotationEffectMenu_->addAction(setAnnotationEffectToBlurAct_);
  }

  appLanguageMenu_ = new QMenu(this); {
    UiStyle::globalInstance()->setContextMenuStyle(appLanguageMenu_, true); // persistent = true

    appLanguageMenu_->setTitle("Language ..."); // no tr wrapping "language"
    appLanguageMenu_->setToolTip(TR(T_TOOLTIP_APPLANGUAGE));

    appLanguageMenu_->addAction(setAppLanguageToEnglishAct_);
    appLanguageMenu_->addAction(setAppLanguageToJapaneseAct_);
    //appLanguageMenu_->addAction(setAppLanguageToTraditionalChineseAct_); // Temporarily disabled since no traditional chinese at this time
    appLanguageMenu_->addAction(setAppLanguageToSimplifiedChineseAct_);
  }

  userLanguageMenu_ = new QMenu(this); {
    UiStyle::globalInstance()->setContextMenuStyle(userLanguageMenu_, true); // persistent = true

    userLanguageMenu_->setTitle(TR(T_MENUTEXT_USERLANGUAGE) + " ...");
    userLanguageMenu_->setToolTip(TR(T_TOOLTIP_USERLANGUAGE));

    userLanguageMenu_->addAction(setUserLanguageToEnglishAct_);
    userLanguageMenu_->addAction(setUserLanguageToJapaneseAct_);
    userLanguageMenu_->addAction(setUserLanguageToChineseAct_);
    userLanguageMenu_->addAction(setUserLanguageToKoreanAct_);
    userLanguageMenu_->addAction(setUserLanguageToUnknownAct_);
  }

  annotationLanguageMenu_ = new QMenu(this); {
    UiStyle::globalInstance()->setContextMenuStyle(annotationLanguageMenu_, true); // persistent = true

    annotationLanguageMenu_->setTitle(TR(T_MENUTEXT_ANNOTATIONLANGUAGE) + " ...");
    annotationLanguageMenu_->setToolTip(TR(T_TOOLTIP_ANNOTATIONLANGUAGE));

    annotationLanguageMenu_->addAction(toggleAnnotationLanguageToAnyAct_);
    annotationLanguageMenu_->addAction(toggleAnnotationLanguageToUnknownAct_);
    annotationLanguageMenu_->addSeparator();
    annotationLanguageMenu_->addAction(toggleAnnotationLanguageToEnglishAct_);
    annotationLanguageMenu_->addAction(toggleAnnotationLanguageToJapaneseAct_);
    annotationLanguageMenu_->addAction(toggleAnnotationLanguageToChineseAct_);
    annotationLanguageMenu_->addAction(toggleAnnotationLanguageToKoreanAct_);
  }

#ifdef Q_WS_WIN
  if (!QtWin::isWindowsVistaOrLater())
#endif // Q_WS_WIN
  {
    themeMenu_ = new QMenu(this);

    UiStyle::globalInstance()->setContextMenuStyle(themeMenu_, true); // persistent = true
    themeMenu_->setTitle(TR(T_MENUTEXT_THEME) + " ...");
    themeMenu_->setToolTip(TR(T_TOOLTIP_THEME));

    themeMenu_->addAction(setThemeToDefaultAct_);
    themeMenu_->addAction(setThemeToRandomAct_);
    themeMenu_->addSeparator();
    themeMenu_->addAction(setThemeToDarkAct_);
    themeMenu_->addAction(setThemeToBlackAct_);
    themeMenu_->addAction(setThemeToBlueAct_);
    themeMenu_->addAction(setThemeToBrownAct_);
    themeMenu_->addAction(setThemeToCyanAct_);
    themeMenu_->addAction(setThemeToGrayAct_);
    themeMenu_->addAction(setThemeToGreenAct_);
    themeMenu_->addAction(setThemeToPinkAct_);
    themeMenu_->addAction(setThemeToPurpleAct_);
    themeMenu_->addAction(setThemeToRedAct_);
    themeMenu_->addAction(setThemeToWhiteAct_);
    themeMenu_->addAction(setThemeToYellowAct_);
  }

  subtitleStyleMenu_ = new QMenu(this); {
    UiStyle::globalInstance()->setContextMenuStyle(subtitleStyleMenu_, true); // persistent = true

    subtitleStyleMenu_->setTitle(TR(T_MENUTEXT_SUBTITLESTYLE) + " ...");
    subtitleStyleMenu_->setToolTip(TR(T_TOOLTIP_SUBTITLESTYLE));

    subtitleStyleMenu_->addAction(setSubtitleColorToDefaultAct_);
    subtitleStyleMenu_->addSeparator();
    subtitleStyleMenu_->addAction(setSubtitleColorToWhiteAct_);
    subtitleStyleMenu_->addAction(setSubtitleColorToCyanAct_);
    subtitleStyleMenu_->addAction(setSubtitleColorToBlueAct_);
    subtitleStyleMenu_->addAction(setSubtitleColorToPurpleAct_);
    subtitleStyleMenu_->addAction(setSubtitleColorToRedAct_);
    subtitleStyleMenu_->addAction(setSubtitleColorToOrangeAct_);
    subtitleStyleMenu_->addAction(setSubtitleColorToBlackAct_);
  }

  browseMenu_ = new QMenu(this); {
    UiStyle::globalInstance()->setContextMenuStyle(browseMenu_, true); // persistent = true

    browseMenu_->setTitle(TR(T_MENUTEXT_BROWSE) + " ...");
    browseMenu_->setToolTip(TR(T_TOOLTIP_BROWSE));
  }

  trackMenu_ = new QMenu(this); {
    UiStyle::globalInstance()->setContextMenuStyle(trackMenu_, true); // persistent = true

    trackMenu_->setTitle(TR(T_MENUTEXT_TRACK) + " ...");
    trackMenu_->setToolTip(TR(T_TOOLTIP_TRACK));
  }

  recentMenu_ = new QMenu(this); {
    UiStyle::globalInstance()->setContextMenuStyle(recentMenu_, true); // persistent = true
    recentMenu_->setTitle(TR(T_MENUTEXT_RECENT) + " ...");
    recentMenu_->setToolTip(TR(T_TOOLTIP_RECENT));
  }

  playlistMenu_ = new QMenu(this); {
    UiStyle::globalInstance()->setContextMenuStyle(playlistMenu_, true); // persistent = true
    playlistMenu_->setTitle(TR(T_MENUTEXT_PLAYLIST) + " ...");
    playlistMenu_->setToolTip(TR(T_TOOLTIP_PLAYLIST));
  }

  backwardMenu_ = new QMenu(this); {
    backwardMenu_->setIcon(QIcon(RC_IMAGE_BACKWARD));
    backwardMenu_->setTitle(TR(T_MENUTEXT_BACKWARD) + " ...");
    backwardMenu_->setToolTip(TR(T_TOOLTIP_BACKWARD));
    UiStyle::globalInstance()->setContextMenuStyle(backwardMenu_, true); // persistent = true

    backwardMenu_->addAction(backward5sAct_);
    backwardMenu_->addAction(backward30sAct_);
    backwardMenu_->addAction(backward90sAct_);
    backwardMenu_->addAction(backward5mAct_);
    backwardMenu_->addAction(backward10mAct_);
  }

  forwardMenu_ = new QMenu(this); {
    forwardMenu_->setIcon(QIcon(RC_IMAGE_FORWARD));
    forwardMenu_->setTitle(TR(T_MENUTEXT_FORWARD) + " ...");
    forwardMenu_->setToolTip(TR(T_TOOLTIP_FORWARD));
    UiStyle::globalInstance()->setContextMenuStyle(forwardMenu_, true); // persistent = true

    forwardMenu_->addAction(forward5sAct_);
    forwardMenu_->addAction(forward30sAct_);
    forwardMenu_->addAction(forward90sAct_);
    forwardMenu_->addAction(forward5mAct_);
    forwardMenu_->addAction(forward10mAct_);
  }

  openMenu_ = new QMenu(this); {
    openMenu_->setTitle(TR(T_MENUTEXT_OPENCONTEXTMENU) + " ...");
    openMenu_->setToolTip(TR(T_TOOLTIP_OPENCONTEXTMENU));
    openMenu_->setIcon(QIcon(RC_IMAGE_OPENCONTEXTMENU));
    UiStyle::globalInstance()->setContextMenuStyle(openMenu_, true); // persistent = true
  }

  subtitleMenu_ = new QMenu(this); {
    subtitleMenu_->setIcon(QIcon(RC_IMAGE_SUBTITLE));
    subtitleMenu_->setTitle(TR(T_MENUTEXT_SUBTITLE) + " ...");
    subtitleMenu_->setToolTip(TR(T_TOOLTIP_SUBTITLE));
    UiStyle::globalInstance()->setContextMenuStyle(subtitleMenu_, true); // persistent = true
  }

  playMenu_ = new QMenu(this); {
    playMenu_->setTitle(tr("Play") + " ...");
    playMenu_->setToolTip(tr("Play menu"));
    UiStyle::globalInstance()->setContextMenuStyle(playMenu_, true); // persistent = true
  }

  audioTrackMenu_ = new QMenu(this); {
    audioTrackMenu_->setIcon(QIcon(RC_IMAGE_AUDIOTRACK));
    audioTrackMenu_->setTitle(TR(T_MENUTEXT_AUDIOTRACK) + " ...");
    audioTrackMenu_->setToolTip(TR(T_TOOLTIP_AUDIOTRACK));
    UiStyle::globalInstance()->setContextMenuStyle(audioTrackMenu_, true); // persistent = true
  }

  annotationSubtitleMenu_ = new QMenu(this); {
    annotationSubtitleMenu_->setIcon(QIcon(RC_IMAGE_ANNOTSUBTITLE));
    annotationSubtitleMenu_->setTitle(TR(T_MENUTEXT_ANNOTSUBTITLE) + " ...");
    annotationSubtitleMenu_->setToolTip(TR(T_TOOLTIP_ANNOTSUBTITLE));
    UiStyle::globalInstance()->setContextMenuStyle(annotationSubtitleMenu_, true); // persistent = true

    annotationSubtitleMenu_->addAction(toggleSubtitleAnnotationVisibleAct_);
    annotationSubtitleMenu_->addSeparator();
    annotationSubtitleMenu_->addAction(toggleNonSubtitleAnnotationVisibleAct_);
    annotationSubtitleMenu_->addAction(toggleSubtitleOnTopAct_);
    annotationSubtitleMenu_->addMenu(subtitleStyleMenu_);
  }

  sectionMenu_ = new QMenu(this); {
    sectionMenu_->setIcon(QIcon(RC_IMAGE_SECTION));
    sectionMenu_->setTitle(TR(T_MENUTEXT_SECTION) + " ...");
    sectionMenu_->setToolTip(TR(T_TOOLTIP_SECTION));
    UiStyle::globalInstance()->setContextMenuStyle(sectionMenu_, true); // persistent = true
  }

//#ifndef Q_WS_WIN
  // Use menuBar_ instead MainWindow::menuBar() to enable customization.
  // It is important to create the menubar WITHOUT PARENT, so that diff windows could share the same menubar.
  // See: http://doc.qt.nokia.com/latest/mac-differences.html#menu-bar
  // See: http://doc.qt.nokia.com/stable/qmainwindow.html#menuBar

  // jichi 11/11/11:  It's important not to use act like quitAct_ or aboutAct_ with translations.

  //menuBar_ = new QMenuBar;
  fileMenu_ = menuBar()->addMenu(tr("&File")); {
    fileMenu_->addAction(openAct_);
    fileMenu_->addAction(openUrlAct_);
    fileMenu_->addSeparator();
    fileMenu_->addAction(openDeviceAct_);
    fileMenu_->addAction(openVideoDeviceAct_);
    fileMenu_->addAction(openSubtitleAct_);
    fileMenu_->addAction(openAnnotationUrlAct_);
#ifdef Q_WS_WIN // TODO add support for Mac/Linux
    fileMenu_->addAction(openAudioDeviceAct_);
#endif // Q_WS_WIN
    fileMenu_->addSeparator();
    fileMenu_->addAction(tr("&Quit"), this, SLOT(close())); // DO NOT TRANSLATE ME
  }

  //editMenu = menuBar()->addMenu(tr("&Edit"));
  //editMenu->addAction(undoAct);
  //editMenu->addAction(insertAct);
  //editMenu->addAction(analyzeAct);
  //editMenu->addAction(transformAct);
  //editMenu->addAction(applyAct);
  //editMenu->addAction(generateAct);
  //editMenu->addAction(clearAct);

  //viewMenu_ = menuBar()->addMenu(tr("&View"));

  helpMenu_ = menuBar()->addMenu(tr("&Help"));
  helpMenu_->addAction(tr("&Help"), this, SLOT(help())); // DO NOT TRANSLATE ME
  helpMenu_->addAction(tr("&About"), this, SLOT(about())); // DO NOT TRANSLATE ME
//#endif // !Q_WS_WIN
}

// jichi:10/17/2011: TODO: Totally remove dock widgets; use layout instead
void
MainWindow::createDockWindows()
{
  // Main view
  setCentralWidget(videoView_);

  // Osd view
  //osdDock_ = new OsdDock(this);
  //osdDock_->setWidget(osdWindow_);
  //embeddedPlayer_->hide();
  //annotationView_->setFullScreenView(osdDock_);
  //osdDock_->showFullScreen();

  // Main player
  MainPlayerDock *mainPlayerDock  = new MainPlayerDock(this);
  mainPlayerDock->setWidget(mainPlayer_);
  addDockWidget(Qt::BottomDockWidgetArea, mainPlayerDock);

  // Mini player
  //miniPlayerDock_ = new MiniPlayerDock(this);
  //miniPlayerDock_->setWidget(miniPlayer_);
  //miniPlayer_->setDockWidget(miniPlayerDock_);
  //miniPlayerDock_->hide();
  //viewMenu_->addAction(miniPlayerDock_->toggleViewAction());

  //userPanelDock_->setWidget(userPanel_);
  //userPanel_->setDockWidget(userPanelDock_);
  //userPanelDock_->hide();
  //viewMenu_->addAction(userPanelDock_->toggleViewAction());
}


// - Hub events -

void
MainWindow::updateTokenMode()
{
  // Stop
  if (hub_->isMediaTokenMode() && !player_->isStopped())
    player_->stop();

#ifdef USE_WIN_QTH
  if (!hub_->isSignalTokenMode())
    messageHandler_->setActive(false);
#endif // USE_WIN_QTH

  if (!hub_->isLiveTokenMode())
    closeChannel();

  annotationView_->invalidateAnnotations();

  // Restart

  switch (hub_->tokenMode()) {
  case SignalHub::MediaTokenMode:
    break;
  case SignalHub::LiveTokenMode:
  case SignalHub::SignalTokenMode:
    if (annotationView_->trackedWindow())
      hub_->setEmbeddedPlayerMode();
    break;
  }

  if (!hub_->isMediaTokenMode())
    hub_->play();
}

void
MainWindow::updatePlayMode()
{
  //switch (hub_->playerMode()) {
  //case SignalHub::LivePlayMode:
  //  openChannel();
  //  break;
  //case SignalHub::NormalPlayMode:
  //default:
  //  closeChannel();
  //}
}

void
MainWindow::invalidatePlayerMode()
{
  switch (hub_->playerMode()) {
  case SignalHub::NormalPlayerMode:
  case SignalHub::MiniPlayerMode:
    if (annotationView_->trackedWindow())
      hub_->setEmbeddedPlayerMode();
    break;
  case SignalHub::EmbeddedPlayerMode:
    if (!annotationView_->trackedWindow())
      //hub_->setNormalPlayerMode();
      hub_->stop();
    break;
  }
}

void
MainWindow::updatePlayerMode()
{
  switch (hub_->playerMode()) {
  case SignalHub::NormalPlayerMode:
    embeddedPlayer_->hide();
    miniPlayer_->hide();
    mainPlayer_->show();
    if (hub_->isNormalWindowMode())
      showNormal();
    else
      show();
    break;

  case SignalHub::MiniPlayerMode:
    //setVisible(v);
    mainPlayer_->hide();
    embeddedPlayer_->hide();
    miniPlayer_->show();
    if (!annotationView_->trackedWindow()) {
      if (hub_->isNormalWindowMode())
        showNormal();
      else
        show();
    }
    break;

  case SignalHub::EmbeddedPlayerMode:
    //setVisible(m);
    if (annotationView_->trackedWindow() ||
        hub_->isLiveTokenMode() && hub_->isFullScreenWindowMode())
      hide();
    mainPlayer_->hide();
    miniPlayer_->hide();
    embeddedPlayer_->show();
    osdWindow_->raise();
    break;
  }

  annotationView_->invalidateGeometry();
}

void
MainWindow::updateWindowMode()
{
  switch (hub_->windowMode()) {
  case SignalHub::FullScreenWindowMode:
#ifdef USE_WIN_DWM
    if (UiStyle::isAeroAvailable())
      UiStyle::globalInstance()->setWindowDwmEnabled(this, false);
#endif // USE_WIN_DWM
#ifdef Q_WS_X11
    if (hub_->isMediaTokenMode() && player_->hasMedia())
      UiStyle::globalInstance()->setBlackBackground(this);
#endif // Q_WS_X11

    if (mainPlayer_->isVisible())
      mainPlayer_->hide();

    switch (hub_->tokenMode()) {
    case SignalHub::MediaTokenMode:
      showFullScreen();
      if (hub_->isNormalPlayerMode())
        hub_->setEmbeddedPlayerMode();
      annotationView_->setFullScreenMode();
      break;
    case SignalHub::LiveTokenMode:
    case SignalHub::SignalTokenMode:
      hub_->setEmbeddedPlayerMode(); // always embed in full screen signal mode
      annotationView_->setFullScreenMode();
      break;
    }
    break;

  case SignalHub::NormalWindowMode:
    if (embeddedPlayer_->isVisible())
      embeddedPlayer_->hide();

    annotationView_->setFullScreenMode(false);

    if (!annotationView_->trackedWindow())
      hub_->setNormalPlayerMode();
    else {
      hide();
      updatePlayerMode();
    }
    break;
  }
}

/*
void
MainWindow::setVisible(bool visible)
{
  if (visible == isVisible())
    return;

  Base::setVisible(visible);

  if (!hub_->isFullScreenWindowMode()) {
#ifdef USE_WIN_DWM
    if (UiStyle::isAeroAvailable())
      UiStyle::globalInstance()->setWindowDwmEnabled(this, true);
    else
#endif // USE_WIN_DWM
      UiStyle::globalInstance()->setWindowBackground(this, false); // persistent is false
  }
}
*/

// - Open file -

void
MainWindow::open()
{
  switch (hub_->tokenMode()) {
  case SignalHub::LiveTokenMode:
  case SignalHub::MediaTokenMode: openFile(); break;
  case SignalHub::SignalTokenMode: openProcess(); break;
  }
}

void
MainWindow::openUrl()
{
  if (!mediaUrlDialog_) {
    mediaUrlDialog_ = new MediaUrlDialog(this);
    connect(mediaUrlDialog_, SIGNAL(urlEntered(QString)), SLOT(openUrl(QString)));
  }
  mediaUrlDialog_->show();
}

void
MainWindow::openAnnotationUrl()
{
  if (!annotationUrlDialog_) {
    annotationUrlDialog_ = new SubUrlDialog(this);
    connect(annotationUrlDialog_, SIGNAL(urlEntered(QString,bool)), SLOT(openAnnotationUrl(QString,bool)));
  }
  annotationUrlDialog_->show();
}

void
MainWindow::openAnnotationUrlFromAliases(const AliasList &l)
{
  if (!l.isEmpty())
    foreach (Alias a, l)
      if (a.type() == Alias::AT_Url)
        openAnnotationUrl(a.text());
}

void
MainWindow::openAnnotationUrl(const QString &url, bool save)
{
  log(tr("analyzing URL ...") + " " + url);
  if (registerAnnotationUrl(url)) {
    bool ok = mrlResolver_->resolveSubtitle(url);
    if (!ok)
      warn(tr("failed to resolve URL") + ": " + url);
    else if (save)
      submitAliasText(url, Alias::AT_Url);
  }
}

void
MainWindow::openUrl(const QString &url)
{
  if (mrlResolver_->resolveMedia(url))
    log(tr("analyzing URL ...") + " " + url);
  else {
    if (isRemoteMrl(url))
      openMrl(url, false); // checkPath = false
    else
      openLocalUrl(url);
  }
}

void
MainWindow::openFile()
{
  if (!QDir(recentPath_).exists())
    recentPath_.clear();

  static const QString filter =
      TR(T_FORMAT_SUPPORTED) + ("(" G_FORMAT_SUPPORTED ")" ";;")
#ifdef USE_MODE_SIGNAL
    + TR(T_FORMAT_PROGRAM)   + ("(" G_FORMAT_PROGRAM ")" ";;")
#endif // USE_MODE_SIGNAL
    + TR(T_FORMAT_VIDEO)     + ("(" G_FORMAT_VIDEO ")" ";;")
    + TR(T_FORMAT_AUDIO)     + ("(" G_FORMAT_AUDIO ")" ";;")
    + TR(T_FORMAT_PICTURE)   + ("(" G_FORMAT_PICTURE ")" ";;")
    + TR(T_FORMAT_ALL)       + ("(" G_FORMAT_ALL ")");

  QStringList l = QFileDialog::getOpenFileNames(
        this, TR(T_TITLE_OPENFILE), recentPath_, filter);

  if (!l.isEmpty()) {
    playlist_ = l;

    QString fileName = l.front();
    recentPath_ = QFileInfo(fileName).absolutePath();
    openSource(fileName);
  }
}

void
MainWindow::openProcess()
{
#ifdef USE_MODE_SIGNAL
  showSignalView();
#endif // USE_MODE_SIGNAL
}

void
MainWindow::openWindow()
{
#ifdef USE_MODE_SIGNAL
  setProcessPickDialogVisible(true);
#endif // USE_MODE_SIGNAL
}

bool
MainWindow::isDevicePath(const QString &path)
{
#ifdef Q_WS_WIN
  if (path.contains(QRegExp("^[a-zA-Z]:$")) ||
      path.contains(QRegExp("^[a-zA-Z]:\\\\$")))
    return true;
#endif // Q_WS_MAC
#ifdef Q_OS_UNIX
  if (QtUnix::isDeviceFile(path))
    return true;
#endif // Q_OS_UNIX
  return false;
}

void
MainWindow::openDevice()
{
  if (!deviceDialog_) {
    deviceDialog_ = new DeviceDialog(this);
    connect(deviceDialog_, SIGNAL(deviceSelected(QString,bool)), SLOT(openDevicePath(QString,bool)));
  }
  deviceDialog_->show();
}

void
MainWindow::openVideoDevice()
{
  if (!QDir(recentPath_).exists())
    recentPath_.clear();

  QString path = QFileDialog::getExistingDirectory(
        this, TR(T_TITLE_OPENVIDEODEVICE), recentPath_);

  if (!path.isEmpty()) {
    playlist_.clear();

    recentPath_ = path;
    openDevicePath(path, false); // isAudioCD == false
  }
}

void
MainWindow::openAudioDevice()
{
  if (!QDir(recentPath_).exists())
    recentPath_.clear();

  QString path = QFileDialog::getExistingDirectory(
        this, TR(T_TITLE_OPENAUDIODEVICE), recentPath_);

  if (!path.isEmpty()) {
    playlist_.clear();

    recentPath_ = path;
    openDevicePath(path, true); // isAudioCD == true
  }
}

void
MainWindow::openDevicePath(const QString &path, bool isAudioCD)
{
  //if (!isDevicePath(path)) {
  //  warn(TR(T_ERROR_BAD_DEVICEPATH) + ": " + path);
  //  return;
  //}

  playlist_.clear();

  QString mrl = path;
  if (isAudioCD && !mrl.startsWith(PLAYER_URL_CD, Qt::CaseInsensitive))
    mrl = PLAYER_URL_CD + path;

  recentSourceLocked_ = false;
  openMrl(mrl);
}

void
MainWindow::openDirectory()
{
  if (!QDir(recentPath_).exists())
    recentPath_.clear();

  QString path = QFileDialog::getExistingDirectory(
        this, TR(T_TITLE_OPENAUDIODEVICE), recentPath_);

  if (!path.isEmpty()) {
    playlist_.clear();

    recentPath_ = path;
    recentSourceLocked_ = false;
    openMrl(path);
  }
}

void
MainWindow::openSubtitle()
{
  if (!QDir(recentPath_).exists())
    recentPath_.clear();

  static const QString filter =
      TR(T_FORMAT_SUBTITLE)  + ("(" G_FORMAT_SUBTITLE ")" ";;")
    + TR(T_FORMAT_ALL)       + ("(" G_FORMAT_ALL ")");

  QStringList l = QFileDialog::getOpenFileNames(
        this, TR(T_TITLE_OPENSUBTITLE), recentPath_, filter);

  if (!l.isEmpty()) {
    recentPath_ = QFileInfo(l.front()).absolutePath();
    foreach (QString fileName, l)
      openSubtitlePath(fileName);
  }
}

void
MainWindow::openLocalUrl(const QUrl &url)
{
  if (!url.isValid())
    return;

  recentSourceLocked_ = false;
  QString path = url.toLocalFile();
  if (!path.isEmpty())
    openSource(path);
  else
    openMrl(url.toEncoded(), false); // check path = false
}

void
MainWindow::openLocalUrls(const QList<QUrl> &urls)
{
  // FIXME: add to playlist rather then this approach!!!!!
  if (!urls.empty()) {
    playlist_.clear();
    foreach (const QUrl &url, urls) {
      QString path = url.toLocalFile();
      if (!path.isEmpty())
        playlist_.append(path);
    }
    openLocalUrl(urls.front());
  }
}

void
MainWindow::openMimeData(const QMimeData *mime)
{
  recentSourceLocked_ = false;
  if (mime && mime->hasUrls())
    openLocalUrls(mime->urls());
}


void
MainWindow::setRecentOpenedFile(const QString &path)
{
  recentOpenedFile_ = path;
  if (playlist_.size() > 1)
    invalidatePlaylistMenu();
}

void
MainWindow::openRemoteMedia(const MediaInfo &mi, QNetworkCookieJar *cookieJar)
{
  DOUT("refurl =" << mi.refurl << ", mrls.size =" << mi.mrls.size());
  recentSourceLocked_ = false;
  playlist_.clear();
  if (mi.mrls.isEmpty())
    return;
  if (mi.mrls.size() > 1) {
    foreach (const MrlInfo &mrl, mi.mrls)
      playlist_.append(mrl.url);
  }

  //if (nam && mi.mrls.size() > 1) {
  //  if (!mi.title.isEmpty() && !isRemoteMrl(mi.title))
  //    player_->setMediaTitle(mi.title);
  //  recentDigest_.clear();
  //  recentSource_.clear();
//
  //  MrlInfo mrl = mi.mrls.front();
  //  DOUT("mrl =" << mrl.url);
//
  //  MediaStreamInfo msi;
  //  foreach (const MrlInfo &mrl, mi.mrls) {
  //    if (mrl.url.isEmpty())
  //      continue;
  //    msi.urls.append(mrl.url);
  //    msi.durations.append(mrl.duration);
  //    msi.sizes.append(mrl.size);
  //  }
  //  MediaStreamer::globalInstance()->stream(msi, nam);
  //  return;
  //}

  if (cookieJar)
    player_->setCookieJar(cookieJar);

  openMrl(mi.mrls.front().url, false); // checkPath = false
  if (!mi.title.isEmpty() && !isRemoteMrl(mi.title))
    player_->setMediaTitle(mi.title);

  recentDigest_.clear();
  recentSource_.clear();
  if (!mi.refurl.isEmpty()) {
    addRecent(mi.refurl);
    recentSource_ = mi.refurl;
    recentSourceLocked_ = true;
    setToken();
  }

  //importAnnotationsFromUrl(mi.suburl);
  if (!mi.suburl.isEmpty())
    QTimer::singleShot(5000, // TODO: use event loop in aother thread rather than 5 sec
      new MainWindow_slot_::ImportAnnotationsFromUrl(mi.suburl, this),
      SLOT(importAnnotationsFromUrl())
    );
}

void
MainWindow::closeMedia()
{
  if (player_->hasMedia())
    player_->closeMedia();

  dataManager_->token().setId(0);
  dataManager_->removeAliases();
  annotationView_->invalidateAnnotations();
}

void
MainWindow::openStreamUrl(const QString &rtsp)
{ openMrl(rtsp, false); }

void
MainWindow::openMrl(const QString &path, bool checkPath)
{
  DOUT("enter: path =" << path);
  bool fullScreen = hub_->isLiveTokenMode() && hub_->isEmbeddedPlayerMode();
  hub_->setMediaTokenMode();
  if (fullScreen) {
    hub_->setFullScreenWindowMode();
    updateWindowMode();
  }

  recentDigest_.clear();
  if (!recentSourceLocked_)
    recentSource_.clear();

  setRecentOpenedFile(path);

  setBrowsedFile(path);

  if (checkPath && !isRemoteMrl(path)) {
    QFileInfo fi(removeUrlPrefix(path));
    if (!fi.exists()) {
      warn(TR(T_ERROR_BAD_FILEPATH) + ": " + path);
      DOUT("exit: path not exist: " + path);
      return;
    }
  }

  tokenType_ = fileType(path);

  if (!player_->isValid())
    resetPlayer();

  DOUT("invoke openMedia");
  if (player_->hasMedia())
    closeMedia();
  player_->openMedia(path);

  if (!isRemoteMrl(path))
    addRecent(path);
  if (player_->hasPlaylist()) {
    DOUT("exit: media has playlist");
    return;
  }

  if (player_->hasMedia()) {
    if (player_->isMouseEventEnabled())
      player_->startVoutTimer();
    DOUT("invoke invalidateMediaAndPlay");
    invalidateMediaAndPlay();
  }

  DOUT("exit");
}

bool
MainWindow::isPosOutOfScreen(const QPoint &globalPos) const
{
  QRect screen = QApplication::desktop()->availableGeometry(this);
  return !screen.contains(globalPos);
}

bool
MainWindow::isRectOutOfScreen(const QRect &globalRect) const
{
  QRect screen = QApplication::desktop()->availableGeometry(this);
  return !screen.intersects(globalRect);
}

bool
MainWindow::isDigestReady(const QString &path) const
{
  if (path.isEmpty())
    return false;
  if (isRemoteMrl(path))
    return false;
  if (Player::isSupportedPlaylist(path))
    return false;
  if (path.endsWith(".cda", Qt::CaseInsensitive))
    return false;

//#ifdef Q_WS_WIN
//  if (path.endsWith(':') || path.endsWith(":\\") || path.endsWith(":/"))
//    if (path.startsWith(PLAYER_URL_CD) ||
//        dataManager_->token().type() == Token::TT_Audio)
//      return false;
//#endif // Q_WS_WIN

  return true;
}

bool
MainWindow::isAliasReady(const QString &path) const
{
  QFileInfo fi(path);
  if (fi.isDir())
    return false;
  if (isDevicePath(path))
    return false;
  return true;
}

QString
MainWindow::digestFromFile(const QString &path)
{
  if (isRemoteMrl(path))
    return QString();
  QString digest = Token::digestFromFile(path);
  if (digest.isEmpty()) {
    QString localPath = QUrl(path).toLocalFile();
    QString decodedLocalPath = QUrl::fromPercentEncoding(localPath.toAscii());
    digest = Token::digestFromFile(decodedLocalPath);
  }
  return digest;
}

void
MainWindow::invalidateMediaAndPlay(bool async)
{
  DOUT("enter: async =" << async);
  if (disposed_) {
    DOUT("exit: returned from disposed branch");
    return;
  }

  if (player_->hasMedia()) {
    if (async) {
      log(tr("analyzing media ...")); // might cause parallel contension
      QThreadPool::globalInstance()->start(new task_::invalidateMediaAndPlay(this));
      DOUT("exit: returned from async branch");
      return;
    }

    DOUT("playerMutex locking");
    playerMutex_.lock();
    DOUT("playerMutex locked");

    QString path = player_->mediaPath();
    invalidateToken(path);

    if (!isRemoteMrl(path))
      addRecent(path);
    //if (mode_ == NormalPlayMode)
    //  videoView_->resize(INIT_WINDOW_SIZE);

    player_->play();
    if (!isRemoteMrl(path))
      QTimer::singleShot(0, player_, SLOT(loadExternalSubtitles())); // load external subtitles later

    invalidateWindowTitle();

#ifdef Q_WS_MAC
    // ensure videoView_ stretch in full screen mode
    if (hub_->isEmbeddedPlayerMode() && !videoView_->isViewVisible()) {
      videoView_->showView();
      hub_->setEmbeddedPlayerMode(false);
      hub_->setEmbeddedPlayerMode(true);
    }
#endif // Q_WS_MAC

#ifdef Q_WS_X11
    if (hub_->isFullScreenWindowMode())
      UiStyle::globalInstance()->setBlackBackground(this);
#endif // Q_WS_X11

    // SingleShot only because of multi-threading problem.
    QTimer::singleShot(0, this, SLOT(invalidateTrackMenu()));

    DOUT("playerMutex unlocking");
    playerMutex_.unlock();
    DOUT("playerMutex unlocked");
  }
  DOUT("exit");
}

void
MainWindow::openSubtitlePath(const QString &path)
{
  if (!player_->hasMedia()) {
    log(TR(T_ERROR_NO_MEDIA))   ;
    return;
  }

  if (!QFileInfo(path).exists()) {
    warn(TR(T_ERROR_BAD_FILEPATH) + ": " + path);
    return;
  }

  bool ok = player_->openSubtitle(path);
  if (!ok)
    warn(TR(T_ERROR_BAD_SUBTITLE) + ": " + path);
}

void
MainWindow::nextFrame()
{
  if (hub_->isMediaTokenMode() && player_->hasMedia())
    player_->nextFrame();
}

void
MainWindow::showMinimizedAndPause()
{
  showMinimized();
  pause();
}

void
MainWindow::pause()
{
  switch (hub_->tokenMode()) {
  case SignalHub::MediaTokenMode:
    if (player_->isPlaying())
      player_->pause();
    break;

  case SignalHub::SignalTokenMode:
  case SignalHub::LiveTokenMode:
    break;
  }
}

void
MainWindow::stop()
{
  switch (hub_->tokenMode()) {
  case SignalHub::MediaTokenMode:
    if (player_->hasMedia() && !player_->isStopped())
      player_->stop();
    break;

  case SignalHub::LiveTokenMode:
    closeChannel();
    //hub_->setMediaTokenMode();
    break;

  case SignalHub::SignalTokenMode:
#ifdef USE_MODE_SIGNAL
    messageHandler_->setActive(false);
    log(": " + tr("detaching all processes ..."));
    QTH->detachAllProcesses();
#endif // USE_MODE_SIGNAL
    hub_->setNormalPlayerMode();
    break;
  }
}

bool
MainWindow::isAutoPlayNext() const
{ return toggleAutoPlayNextAct_->isChecked(); }

void
MainWindow::play()
{
  switch (hub_->tokenMode()) {
  case SignalHub::MediaTokenMode:
    if (player_->hasMedia())
      player_->play();
    else
      open();
    break;

  case SignalHub::LiveTokenMode:
    if (!annotationView_->isStarted() || !liveTimer_->isActive())
      openChannel();
    break;

  case SignalHub::SignalTokenMode:
#ifdef USE_MODE_SIGNAL
    messageHandler_->setActive(true);
#endif // USE_MODE_SIGNAL
    break;
  }
}

void
MainWindow::playPause()
{
  switch (hub_->tokenMode()) {
  case SignalHub::MediaTokenMode:
    if (player_->hasMedia())
      player_->playPause();
    else
      open();
    break;

  case SignalHub::LiveTokenMode:
  case SignalHub::SignalTokenMode:
    if (hub_->isPlaying())
      hub_->pause();
    else
      hub_->play();
    break;
  }
}

void
MainWindow::replay()
{
  if (!hub_->isMediaTokenMode()) {
    play();
    return;
  }

  if (player_->hasMedia()) {
    player_->setTime(0);
    emit seeked();

    // A quick fix to solve delay on position update
    // TO BE REMOVED!
    if (hasVisiblePlayer())
      visiblePlayer()->invalidatePositionSlider();

    if (!player_->isPlaying())
      player_->play();
  }
}

void
MainWindow::snapshot()
{
  switch (hub_->tokenMode()) {
  case SignalHub::MediaTokenMode:
    if (player_->hasMedia() && !player_->isStopped()) {

      QString mediaName = QFileInfo(player_->mediaPath()).fileName();
      QTime time = QtExt::msecs2time(player_->time());

      QString saveName = mediaName + "__" + time.toString("hh_mm_ss_zzz") + ".png";
      QString savePath = grabber_->savePath() + "/" + saveName;

      bool succeed = player_->snapshot(savePath);
      if (succeed)
        say(": " + TR(T_SUCCEED_SNAPSHOT_SAVED), "purple");
      else
        say(": " + TR(T_ERROR_SNAPSHOT_FAILED) + ": " + savePath, "red");
    }
    break;

  case SignalHub::LiveTokenMode:
  case SignalHub::SignalTokenMode:
    if (annotationView_->trackedWindow())
      grabber_->grabWindow(annotationView_->trackedWindow());
    else
      grabber_->grabDesktop();
    log(tr("snapshot saved on the destop"));
    break;
  }
}

void
MainWindow::seek(qint64 time)
{
  DOUT("enter: time =" << time);

  if (!hub_->isMediaTokenMode())
    return;

  if (!player_->hasMedia() || !player_->seekable())
    return;

  if (time <= 0) {
    //pause();
    //player_->setTime(0);
    replay();
    DOUT("exit: time <=0, time =" << time);
    return;
  }

  qint64 length = player_->mediaLength();
  if (time >= length) {
    //pause();
    //player_->setTime(length - 1);
    stop();
    DOUT("exit: time >= length, length =" << length << ", time =" << time);
    return;
  }

  player_->setTime(time);
  emit seeked();

  logger_->logSeeked(time);
  DOUT("exit: time =" << time);
}

void
MainWindow::forward(qint64 delta)
{
  if (!player_->hasMedia() || !player_->seekable())
    return;

  if (delta < 0) {
    backward(- delta);
    return;
  }

  if (!delta)
    delta = G_FORWARD_INTERVAL;

  return seek(player_->time() + delta);
}

void
MainWindow::backward(qint64 delta)
{
  if (!player_->hasMedia() || !player_->seekable())
    return;

  if (delta < 0) {
    forward(- delta);
    return;
  }

  if (!delta)
    delta = G_BACKWARD_INTERVAL;

  return seek(player_->time() - delta);
}

void
MainWindow::volumeUp(qreal delta)
{
  if (!hub_->isMediaTokenMode()) {
    // TODO: change system volume
    return;
  }

  if (delta == 0)
    delta = G_VOLUME_DELTA;

  qreal v_old = hub_->volume();
  qreal v_new = delta + v_old;
  if (v_new <= 0) {
    if (v_old > 0)
      hub_->setVolume(0);
  } else if (v_new >= 1) {
    if (v_old < 1)
      hub_->setVolume(1);
  } else
    hub_->setVolume(v_new);
}

void
MainWindow::volumeDown(qreal delta)
{
  if (!delta)
    delta = G_VOLUME_DELTA;

  volumeUp(- delta);
}

bool
MainWindow::isPlaying() const
{
  return hub_->isMediaTokenMode() ? player_->isPlaying() :
         hub_->isPlaying();
}

 // - Play mode -

/*
void
MainWindow::setPlayMode(PlayMode mode)
{
  if (mode_ == mode)
    return;
  mode_ = mode;

  switch (mode_) {
  case FullScreenPlayMode:
#ifdef USE_WIN_DWM
    setAeroStyleEnabled(false);
#endif // USE_WIN_DWM
    mainPlayer_->hide();
    showFullScreen();
    embeddedPlayer_->setVisible(!miniPlayer_->isVisible());
    break;

  case NormalPlayMode:
    embeddedPlayer_->hide();
    showNormal();
    mainPlayer_->setVisible(!miniPlayer_->isVisible());
    annotationView_->invalidateSize(); // Otherwise the height doesn't look right
#ifdef USE_WIN_DWM
    setAeroStyleEnabled(true);
#endif // USE_WIN_DWM
    break;
  }
}

*/

// - Mini mode -

bool
MainWindow::hasVisiblePlayer() const
{
  return mainPlayer_->isVisible()
      || embeddedPlayer_->isVisible()
      || miniPlayer_->isVisible();
}

PlayerUi*
MainWindow::visiblePlayer()
{
  return mainPlayer_->isVisible()? static_cast<PlayerUi*>(mainPlayer_)
       : embeddedPlayer_->isVisible() ? static_cast<PlayerUi*>(embeddedPlayer_)
       : miniPlayer_->isVisible()? static_cast<PlayerUi*>(miniPlayer_)
       : 0;
}

//void
//MainWindow::leaveMiniMode()
//{ setMiniMode(false); }

/*
void
MainWindow::setMiniMode(bool visible)
{
  //if (miniPlayer_->isVisible() == visible)
  //  return;

  switch (mode_) {
  case NormalPlayMode:
    mainPlayer_->setVisible(!visible);
    miniPlayer_->setVisible(visible);
    break;
  case FullScreenPlayMode:
    embeddedPlayer_->setVisible(!visible);
    miniPlayer_->setVisible(visible);
    break;
  }
}
*/

// - Help -

void
MainWindow::about()
{
  if (!aboutDialog_)
    aboutDialog_ = new AboutDialog(this);
  aboutDialog_->show();
}


void
MainWindow::help()
{
  if (!helpDialog_)
    helpDialog_ = new HelpDialog(this);
  helpDialog_->show();
}

void
MainWindow::update()
{
  log(tr("openning update URL ...") + " " G_UPDATEPAGE_URL);
  QDesktopServices::openUrl(QUrl(G_UPDATEPAGE_URL));
}

// - Update -

void
MainWindow::invalidateWindowTitle()
{
  switch (hub_->tokenMode()) {
  case SignalHub::SignalTokenMode:
#ifdef USE_MODE_SIGNAL
    setWindowTitle(messageHandler_->processInfo().processName);
#endif // USE_MODE_SIGNAL
    break;

  case SignalHub::LiveTokenMode:
    setWindowTitle(TR(T_TITLE_LIVE));
    break;

  case SignalHub::MediaTokenMode:
    if (player_->hasMedia()) {
      QString title = player_->mediaTitle();
      if (title.isEmpty()) {
        title = player_->mediaPath();
        if (!title.isEmpty())
          title = QFileInfo(title).fileName();
      }
      if (title.isEmpty())
        title = TR(T_TITLE_PROGRAM);

      setWindowTitle(title);
    } else
      setWindowTitle(TR(T_TITLE_PROGRAM));
    break;
  }
}

void
MainWindow::syncInputLineText(const QString &text)
{
  if (!mainPlayer_->inputComboBox()->hasFocus())
    mainPlayer_->inputComboBox()->lineEdit()->setText(text);
  if (!miniPlayer_->inputComboBox()->hasFocus())
    miniPlayer_->inputComboBox()->lineEdit()->setText(text);
  if (!embeddedPlayer_->inputComboBox()->hasFocus())
    embeddedPlayer_->inputComboBox()->lineEdit()->setText(text);
}

void
MainWindow::syncPrefixLineText(const QString &text)
{
  if (!mainPlayer_->prefixComboBox()->hasFocus())
    mainPlayer_->prefixComboBox()->lineEdit()->setText(text);
  if (!miniPlayer_->prefixComboBox()->hasFocus())
    miniPlayer_->prefixComboBox()->lineEdit()->setText(text);
  if (!embeddedPlayer_->prefixComboBox()->hasFocus())
    embeddedPlayer_->prefixComboBox()->lineEdit()->setText(text);
}

// - Dialogs -

void
MainWindow::showLoginDialog()
{ setLoginDialogVisible(true); }

void
MainWindow::hideLoginDialog()
{ setLoginDialogVisible(false); }

void
MainWindow::setAnnotationCountDialogVisible(bool visible)
{
  if (!annotationCountDialog_) {
    annotationCountDialog_ = new AnnotationCountDialog(dataManager_, this);
    connect(annotationCountDialog_, SIGNAL(countChanged(int)), annotationFilter_, SLOT(setAnnotationCountHint(int)));
    annotationCountDialog_->setCount(annotationFilter_->annotationCountHint());
  }
  annotationCountDialog_->setVisible(visible);
}

void
MainWindow::setLoginDialogVisible(bool visible)
{
  if (!loginDialog_) {
    loginDialog_ = new LoginDialog(this);
    connect(loginDialog_, SIGNAL(loginRequested(QString, QString)), SLOT(login(QString, QString)));
  }
  if (visible) {
    loginDialog_->setUserName(server_->user().name());
    loginDialog_->setPassword(server_->user().password());
    loginDialog_->show();
    loginDialog_->raise();
  } else
    loginDialog_->hide();
}

void
MainWindow::setUserViewVisible(bool visible)
{
  if (!userView_) {
    userView_ = new UserView(this);
    userView_->setUser(server_->user());

    connect(this, SIGNAL(userChanged(User)), userView_, SLOT(setUser(User)));
  }
  //if (visible) {
  //  loginDialog_->setUserName(server_->user().name());
  //  loginDialog_->setPassword(server_->user().password());
  //  loginDialog_->show();
  //  loginDialog_->raise();
  //} else
  //  loginDialog_->hide();
  userView_->setVisible(visible);
}

void
MainWindow::toggleTokenViewVisible()
{ tokenView_->setVisible(!tokenView_->isVisible()); }

void
MainWindow::toggleAnnotationBrowserVisible()
{ annotationBrowser_->setVisible(!annotationBrowser_->isVisible()); }

void
MainWindow::setBlacklistViewVisible(bool visible)
{
  if (!blacklistView_)
    blacklistView_ = new BlacklistView(annotationFilter_, this);
  blacklistView_->setVisible(visible);
}

BacklogDialog*
MainWindow::backlogDialog()
{
  if (!backlogDialog_) {
    backlogDialog_ = new BacklogDialog(this);
    connect(hub_, SIGNAL(tokenModeChanged(SignalHub::TokenMode)), backlogDialog_, SLOT(clear()));
    connect(player_, SIGNAL(mediaChanged()), backlogDialog_, SLOT(clear()));
    connect(annotationView_, SIGNAL(subtitleAdded(QString)), backlogDialog_, SLOT(appendSubtitle(QString)));
    connect(annotationView_, SIGNAL(annotationAdded(QString)), backlogDialog_, SLOT(appendAnnotation(QString)));
#ifdef USE_MODE_SIGNAL
    connect(messageHandler_, SIGNAL(messageReceivedWithText(QString)), backlogDialog_, SLOT(appendText(QString)));
#endif // USE_MODE_SIGNAL
  }
  return backlogDialog_;
}

void
MainWindow::setBacklogDialogVisible(bool visible)
{ backlogDialog()->setVisible(visible); }

DownloadDialog*
MainWindow::downloadDialog()
{
  if (!downloadDialog_) {
    downloadDialog_ = new DownloadDialog(this);
    connect(downloadDialog_, SIGNAL(downloadFinished(QString,QString)), SLOT(signFileWithUrl(QString,QString)));
    connect(downloadDialog_, SIGNAL(openFileRequested(QString)), SLOT(openMrl(QString)));
  }
  return downloadDialog_;
}

void
MainWindow::setDownloadDialogVisible(bool visible)
{ downloadDialog()->setVisible(visible); }

ConsoleDialog*
MainWindow::consoleDialog()
{
  if (!consoleDialog_) {
    consoleDialog_ = new ConsoleDialog(this);
    connect(LoggerSignals::globalInstance(), SIGNAL(logged(QString)),
            consoleDialog_, SLOT(appendLogText(QString)), Qt::QueuedConnection);
    connect(LoggerSignals::globalInstance(), SIGNAL(warned(QString)),
            consoleDialog_, SLOT(appendLogText(QString)), Qt::QueuedConnection);
    connect(LoggerSignals::globalInstance(), SIGNAL(notified(QString)),
            consoleDialog_, SLOT(appendLogText(QString)), Qt::QueuedConnection);
    connect(LoggerSignals::globalInstance(), SIGNAL(errored(QString)),
            consoleDialog_, SLOT(appendLogText(QString)), Qt::QueuedConnection);

    Application::globalInstance()->addMessageHandler(ConsoleDialog::messageHandler);
  }
  return consoleDialog_;
}

void
MainWindow::setConsoleDialogVisible(bool visible)
{ consoleDialog()->setVisible(visible); }

void
MainWindow::setAnnotationEditorVisible(bool visible)
{
  if (!annotationEditor_) {
    annotationEditor_ = new AnnotationEditor(this);
    connect(annotationEditor_, SIGNAL(textSaved(QString)), SLOT(eval(QString)));
  }
  annotationEditor_->setVisible(visible);
}

void
MainWindow::toggleAnnotationEditorVisible()
{
  bool v = annotationEditor_ && annotationEditor_->isVisible();
  setAnnotationEditorVisible(!v);
}

void
MainWindow::invalidateSiteAccounts()
{
  if (!siteAccountView_)
    return;
  log(tr("site accounts updated"));
  MrlResolverSettings *settings = MrlResolverSettings::globalInstance();
  settings->setNicovideoAccount(
    siteAccountView_->nicovideoAccount().username,
    siteAccountView_->nicovideoAccount().password);
  settings->setBilibiliAccount(
    siteAccountView_->bilibiliAccount().username,
    siteAccountView_->bilibiliAccount().password);
}

void
MainWindow::setSiteAccountViewVisible(bool visible)
{
  if (!siteAccountView_) {
    siteAccountView_ = new SiteAccountView(this);
    connect(siteAccountView_, SIGNAL(accountChanged()), SLOT(invalidateSiteAccounts()));

    std::pair<QString, QString>
    a = Settings::globalInstance()->nicovideoAccount();
    if (!a.first.isEmpty() && !a.second.isEmpty())
      siteAccountView_->setNicovideoAccount(a.first, a.second);

    a = Settings::globalInstance()->bilibiliAccount();
    if (!a.first.isEmpty() && !a.second.isEmpty())
      siteAccountView_->setBilibiliAccount(a.first, a.second);
  }
  siteAccountView_->setVisible(visible);
}

void
MainWindow::openInCloudView(const QString &url)
{
#ifdef USE_MODULE_WEBBROWSER
  if (!cloudView_)
    cloudView_ = new CloudView(this);
  cloudView_->openUrl(url);
  cloudView_->show();
#else
  QDesktopServices::openUrl(url);
#endif // USE_MODULE_WEBBROWSER
}

void
MainWindow::openHomePage()
{ openInCloudView(G_HOMEPAGE); }

void
MainWindow::showSeekDialog()
{ setSeekDialogVisible(true); }

void
MainWindow::hideSeekDialog()
{ setSeekDialogVisible(false); }

void
MainWindow::setSeekDialogVisible(bool visible)
{
  if (!seekDialog_) {
    seekDialog_ = new SeekDialog(this);
    connect(seekDialog_, SIGNAL(seekRequested(qint64)), SLOT(seek(qint64)));
  }

  if (visible) {
    qint64 t = player_->hasMedia() ? player_->time() : 0;
    seekDialog_->setTime(t);
    seekDialog_->show();
    seekDialog_->raise();
  } else
    seekDialog_->hide();
}

void
MainWindow::showLiveDialog()
{ setLiveDialogVisible(true); }

void
MainWindow::hideLiveDialog()
{ setLiveDialogVisible(false); }

void
MainWindow::setLiveDialogVisible(bool visible)
{
  liveDialog_->setVisible(visible);
  if (visible)
    liveDialog_->raise();
}

void
MainWindow::showSyncDialog()
{ setSyncDialogVisible(true); }

void
MainWindow::hideSyncDialog()
{ setSyncDialogVisible(false); }

void
MainWindow::setSyncDialogVisible(bool visible)
{
  if (!syncDialog_)
    syncDialog_ = new SyncDialog(this);

  syncDialog_->setVisible(visible);
  if (visible)
    syncDialog_->raise();
}

//void
//MainWindow::showCommentView()
//{ setCommentViewVisible(true); }
//
//void
//MainWindow::hideCommentView()
//{ setCommentViewVisible(false); }
//
//void
//MainWindow::setCommentViewVisible(bool visible)
//{
//  if (!commentView_)
//    commentView_ = new CommentView(this);
//
//  if (visible && dataManager_->hasToken())
//    commentView_->setTokenId(dataManager_->token().id());
//  commentView_->setVisible(visible);
//  if (visible)
//    commentView_->raise();
//}

void
MainWindow::showWindowPickDialog()
{ setWindowPickDialogVisible(true); }

void
MainWindow::setWindowPickDialogVisible(bool visible)
{
  if (!windowPickDialog_) {
    windowPickDialog_ = new PickDialog(this);
    windowPickDialog_->setMessage(tr("Select annots window"));
    connect(windowPickDialog_, SIGNAL(windowPicked(WId)), annotationView_, SLOT(setTrackedWindow(WId)));
    connect(windowPickDialog_, SIGNAL(windowPicked(WId)), SLOT(setWindowOnTop()));
  }
  windowPickDialog_->setVisible(visible);
  if (visible)
    windowPickDialog_->raise();
}

void
MainWindow::setProcessPickDialogVisible(bool visible)
{
  if (!processPickDialog_) {
    processPickDialog_ = new PickDialog(this);
    processPickDialog_->setMessage(tr("Select process window to open"));

    connect(processPickDialog_, SIGNAL(windowPicked(WId)), annotationView_, SLOT(setTrackedWindow(WId)));
#ifdef USE_MODE_SIGNAL
    connect(processPickDialog_, SIGNAL(windowPicked(WId)), SLOT(openProcessWindow(WId)));
#endif // USE_MODE_SIGNAL
    connect(processPickDialog_, SIGNAL(windowPicked(WId)), SLOT(setWindowOnTop()));
  }
  processPickDialog_->setVisible(visible);
  if (visible)
    processPickDialog_->raise();
}

// - Annotations -

void
MainWindow::submitAliasText(const QString &text, qint32 type, bool async)
{
  if (!server_->isConnected() && server_->isAuthorized()) {
    warn(tr("please log in to save alias online") + ": " + text);
    return;
  }
  const Token &t = dataManager_->token();
  if (!t.hasId() && !t.hasDigest()) {
    warn(tr("alias not saved for unknown media token") + ": " + text);
    return;
  }
  log(tr("saving alias ...") + ": " + text);
  Alias a;
  a.setTokenId(t.id());
  a.setTokenDigest(t.digest());
  a.setTokenPart(t.part());

  a.setText(text);
  a.setType(type);
  a.setUserId(server_->user().id());
  a.setLanguage(server_->user().language());
  a.setUpdateTime(::time(0));
  tokenView_->addAlias(a);
  submitAlias(a, async);
}

void
MainWindow::submitAlias(const Alias &input, bool async)
{
  DOUT("enter: async =" << async << ", text =" << input.text());
  if (disposed_) {
    DOUT("exit: returned from disposed branch");
    return;
  }
  if (dataManager_->aliasConflicts(input)) {
    warn(tr("similar alias already exists") + ": " + input.text());
    DOUT("exit: alias conflics");
    return;
  }
  if (input.type() == Alias::AT_Url) {
    if (!isRemoteMrl(input.text())) {
      warn(tr("source alias is not a valid URL") + ": " + input.text());
      return;
    }
    openAnnotationUrl(input.text());
  }
  if (async) {
    log(tr("connecting server to submit alias ..."));
    QThreadPool::globalInstance()->start(new task_::submitAlias(input, this));
    DOUT("exit: returned from async branch");
    return;
  }

  dataManager_->addAlias(input);

  DOUT("inetMutex locking");
  inetMutex_.lock();
  DOUT("inetMutex locked");

  qint64 id = dataServer_->submitAlias(input);
  Q_UNUSED(id);
  DOUT("alias id =" << id);
  //if (id)
    log(tr("alias saved") + ": " + input.text());
  //else
  //  warn(tr("failed to update alias text") + ": " + a.text());

  DOUT("inetMutex unlocking");
  inetMutex_.unlock();
  DOUT("inetMutex unlocked");
  DOUT("exit");
}

QString
MainWindow::removeUrlPrefix(const QString &url)
{
  if (url.startsWith(PLAYER_URL_CD, Qt::CaseInsensitive))
    return url.mid(::strlen(PLAYER_URL_CD));

  if (url.startsWith(PLAYER_URL_VCD, Qt::CaseInsensitive))
    return url.mid(::strlen(PLAYER_URL_VCD));

  if (url.startsWith(PLAYER_URL_DVD, Qt::CaseInsensitive))
    return url.mid(::strlen(PLAYER_URL_DVD));

  return url;
}

int
MainWindow::fileType(const QString &fileName)
{
  if (fileName.startsWith(PLAYER_URL_CD, Qt::CaseInsensitive))
    return Token::TT_Audio;

  if (isDevicePath(fileName))
    return Token::TT_Video;

#ifdef USE_MODE_SIGNAL
  static const QStringList supportedProgramSuffices =
      QStringList() G_FORMAT_PROGRAM_(<<);
  foreach (QString suffix, supportedProgramSuffices)
    if (fileName.endsWith("." + suffix, Qt::CaseInsensitive))
      return Token::TT_Program;
#endif // USE_MODE_SIGNAL

  if (Player::isSupportedVideo(fileName))
    return Token::TT_Video;
  if (Player::isSupportedAudio(fileName))
    return Token::TT_Audio;
  if (Player::isSupportedPicture(fileName))
    return Token::TT_Picture;

  if (Player::isSupportedPlaylist(fileName))
    return Token::TT_Audio;

  return Token::TT_Null; // Unknown
}

void
MainWindow::invalidateToken(const QString &mrl)
{
  if (!recentDigest_.isEmpty() || isDigestReady(mrl)) {
    dataManager_->token().setId(0);
    dataManager_->removeAliases();
    annotationView_->invalidateAnnotations();

    if (recentDigest_.isEmpty())
      recentDigest_ = digestFromFile(removeUrlPrefix(mrl));
    if (!recentDigest_.isEmpty())
      setToken(mrl);

  } else if (!recentSource_.isEmpty()) {
    dataManager_->token().setId(0);
    dataManager_->removeAliases();
    annotationView_->invalidateAnnotations();
    setToken();

  } else if (player_->trackCount() != 1) {
    dataManager_->token().setId(0);
    dataManager_->removeAliases();
    annotationView_->invalidateAnnotations();
  }

  QTimer::singleShot(0, this, SLOT(resumeAll()));
}

void
MainWindow::resumeAll()
{
  resumeAudioTrackTimer_->start(3); // 3 times
  resumePlayTimer_->start(3); // 3 times
  resumeSubtitleTimer_->start(3); // 3 times
}

void
MainWindow::showVideoViewIfAvailable()
{
#ifdef Q_WS_MAC
  if (tokenType_ != Token::TT_Audio)
    videoView_->showView();
#endif // Q_WS_MAC
}

void
MainWindow::signFileWithUrl(const QString &path, const QString &url, bool async)
{
  DOUT("enter: async =" << async << ", path =" << path << ", url =" << url);
  if (disposed_) {
    DOUT("exit: returned from disposed branch");
    return;
  }
  if (url.isEmpty() || url.size() > Traits::MAX_ALIAS_LENGTH) {
    warn(tr("URL is too long") + ": " + url);
    return;
  }
  if (async) {
    log(tr("signing media ...") + " " + path);
    QThreadPool::globalInstance()->start(new task_::signFileWithUrl(path, url, this));
    DOUT("exit: returned from async branch");
    return;
  }

  QString digest = digestFromFile(path);
  if (digest.isEmpty()) {
    warn(tr("failed to analyze media") + ": " + path);
    DOUT("exit: failed to compute file digest");
    return;
  }

  qint64 now = QDateTime::currentMSecsSinceEpoch() / 1000;

  Token token; {
    token.setUserId(server_->user().id());
    token.setCreateTime(now);
    token.setDigest(digest);
    token.setType(Token::TT_Video);
  }

  Alias srcAlias;
  QString fileName = QFileInfo(path).fileName();
  if (!fileName.isEmpty()) {
    if (fileName.size() > Traits::MAX_ALIAS_LENGTH) {
      fileName = fileName.mid(0, Traits::MAX_ALIAS_LENGTH);
      warn(TR(T_WARNING_LONG_STRING_TRUNCATED) + ": " + fileName);
    }
    srcAlias.setUserId(server_->user().id());
    srcAlias.setType(Alias::AT_Source);
    srcAlias.setLanguage(server_->user().language());
    srcAlias.setText(fileName);
    srcAlias.setUpdateTime(now);
  }

  Alias urlAlias; {
    urlAlias.setType(Alias::AT_Url);
    urlAlias.setUserId(server_->user().id());
    qint32 lang = Alias::guessUrlLanguage(url, server_->user().language());
    urlAlias.setLanguage(lang);
    urlAlias.setText(url);
    urlAlias.setUpdateTime(now);
  }

  AliasList aliases;
  if (srcAlias.hasText())
    aliases.append(srcAlias);
  aliases.append(urlAlias);

  DOUT("inetMutex locking");
  inetMutex_.lock();
  DOUT("inetMutex locked");

  qint64 tid = dataServer_->submitTokenAndAliases(token, aliases);

  DOUT("inetMutex unlocking");
  inetMutex_.unlock();
  DOUT("inetMutex unlocked");

  if (tid)
    emit logged(tr("media signed") + ": " + path);
  else
    emit warned(tr("failed to sign media") + ": " + path);

  DOUT("exit");
}

void
MainWindow::setToken(const QString &input, bool async)
{
  DOUT("enter: async =" << async << ", path =" << input);
  if (disposed_) {
    DOUT("exit: returned from disposed branch");
    return;
  }
  if (async) {
    log(tr("connecting server to query media/game token ..."));
    QThreadPool::globalInstance()->start(new task_::setToken(input, this));
    DOUT("exit: returned from async branch");
    return;
  }

  DOUT("inetMutex locking");
  inetMutex_.lock();
  DOUT("inetMutex locked");

  //Token token = dataManager_->token();
  dataManager_->removeToken();

  Token token;
  Alias alias;

  if (!input.isEmpty()) {
    token.setType(tokenType_ = fileType(input));

    QString path = removeUrlPrefix(input);
    Q_ASSERT(!recentDigest_.isEmpty());
    if (recentDigest_.isEmpty() && isDigestReady(path))
      recentDigest_ = digestFromFile(path);

    if (!recentDigest_.isEmpty()) {
      token.setDigest(recentDigest_);

      // If isAliasReady, make fileName as alias and guess tt.
      // Otherwise, keep token's original tt from tokenView_
      if (isAliasReady(path)) {
        QString fileName = QFileInfo(path).fileName();
        if (!fileName.isEmpty()) {
          if (fileName.size() > Traits::MAX_ALIAS_LENGTH) {
            fileName = fileName.mid(0, Traits::MAX_ALIAS_LENGTH);
            warn(TR(T_WARNING_LONG_STRING_TRUNCATED) + ": " + fileName);
          }

          alias.setType(Alias::AT_Source);
          alias.setLanguage(server_->user().language());
          alias.setText(fileName);
          alias.setUpdateTime(QDateTime::currentMSecsSinceEpoch() / 1000);
        }
      }
    }
  } else if (!recentDigest_.isEmpty()) {
    token.setDigest(recentDigest_);
  } else if (!recentSource_.isEmpty()) {
    token.setSource(recentSource_);
    token.setType(tokenType_ = Token::TT_Video);
    int part = currentPlaylistIndex();
    if (part < 0)
      part = 0;
    token.setPart(part);
    QString title = player_->mediaTitle();
    if (!title.isEmpty() && !isRemoteMrl(title)) {
      alias.setType(Alias::AT_Name);
      alias.setLanguage(server_->user().language());
      alias.setText(title);
      alias.setUpdateTime(QDateTime::currentMSecsSinceEpoch() / 1000);
    }
  }

 if (!token.hasDigest() && !token.hasSource()) {
    DOUT("inetMutex unlocking");
    inetMutex_.unlock();
    DOUT("inetMutex unlocked");
    DOUT("exit from empty digest branch");
    return;
  }

  token.setCreateTime(QDateTime::currentMSecsSinceEpoch() / 1000);

  if (hub_->isMediaTokenMode() && player_->hasMedia() && token.hasDigest()) {
    int part = 0;
    if (player_->titleCount() > 1)
      part = player_->titleId();
    //else if (player_->hasPlaylist())
    //  part = player_->trackNumber();

    if (part < 0)
      part = 0;
    token.setPart(part);
  }

  AliasList aliases;

  qint64 tid = dataServer_->submitTokenAndAlias(token, alias);
  //alias.setTokenId(tid)
  if (tid) {
    Token t = dataServer_->selectTokenWithId(tid);
    if (t.isValid()) {
      //aliases = dataServer_->selectAliasesWithTokenId(tid);
      token = t;
    } //else
      //warn(TR(T_ERROR_SUBMIT_TOKEN) + ": " + input);
  }

  if (!token.isValid() && cache_->isValid() && token.hasDigest()) {
    log(tr("searching for token in cache ..."));
    Token t = cache_->selectTokenWithDigest(token.digest(), token.part());
    if (t.isValid())
      token = t;
  }

  aliases = dataServer_->selectAliasesWithToken(token);

  dataManager_->setToken(token);
  dataManager_->setAliases(aliases);

//  if (commentView_)
//    commentView_->setTokenId(token.id());
#ifdef USE_MODE_SIGNAL
  //signalView_->tokenView()->setAliases(aliases);
#endif // USE_MODE_SIGNAL

  AnnotationList annots = dataServer_->selectAnnotationsWithToken(token);
  //annotationBrowser_->setAnnotations(annots);
  //annotationView_->setAnnotations(annots);

  //if (tokens.size() == 1) {
  //  Token token = tokens.front();
  //  if (!tokenView_->hasToken()) {
  //    tokenView_->setToken(token);
  //    server_->queryClusteredTokensyTokenId(token.id());
  //    return;
  //  }
  //}
  //tokenView_->setClusteredTokens(tokens);
  //foreach (const Token &t, tokens)
  //  server_->queryAnnotationsByTokenId(t.id());

  DOUT("inetMutex unlocking");
  inetMutex_.unlock();
  DOUT("inetMutex unlocked");

  if (!annots.isEmpty()) {
    QString msg = QString("%1 :" HTML_STYLE_OPEN(color:red) "+%2" HTML_STYLE_CLOSE() ", %3")
      .arg(tr("annotations found"))
      .arg(QString::number(annots.size()))
      .arg("http://annot.me");
    emit logged(msg);
    emit setAnnotationsRequested(annots);
  }

  DOUT("exit");
}

void
MainWindow::showText(const QString &text, bool isSigned)
{
  DOUT("enter: text =" << text);
  Annotation annot;
  if (isSigned && server_->isAuthorized()) {
    annot.setUserId(server_->user().id());
    annot.setUserAlias(server_->user().name());
    annot.setUserAnonymous(server_->user().isAnonymous());
  }
  annot.setLanguage(server_->isAuthorized() ?
    server_->user().language() :
    Traits::AnyLanguage
  );
  annot.setText(text);
  annotationView_->showAnnotation(annot);
  DOUT("exit");
}

void
MainWindow::showTextAsSubtitle(const QString &input, bool isSigned)
{
  if (input.isEmpty())
    return;
  QString text = input;
  QString rest;

  // Split long text
  int n = int(annotationView_->width() * 0.9 / G_ANNOT_CHAR_WIDTH);
  if (n && text.size() > n) {
    rest = text.mid(n);
    text = text.left(n);
  }

  Annotation annot;
  if (isSigned && server_->isAuthorized()) {
    annot.setUserId(server_->user().id());
    annot.setUserAlias(server_->user().name());
    annot.setUserAnonymous(server_->user().isAnonymous());
  }
  annot.setLanguage(server_->isAuthorized() ?
    server_->user().language() :
    Traits::AnyLanguage
  );
  QString sub = CORE_CMD_SUB " " + text;
  annot.setText(sub)    ;
  annotationView_->showAnnotation(annot);

  if (!rest.isEmpty())
    showTextAsSubtitle(rest);
}

void MainWindow::submitLiveText(const QString &text, bool async)
{
  DOUT("enter: async =" << async << ", text =" << text);
  if (disposed_) {
    DOUT("exit: returned from disposed branch");
    return;
  }
  if (!server_->isConnected() || !server_->isAuthorized()) {
    showText(text, true); // isSigned = true
    DOUT("exit: returned from showText branch");
    return;
  }

  if (async) {
    log(tr("connecting server to submit annot ..."));
    QThreadPool::globalInstance()->start(new task_::submitLiveText(text, this));
    DOUT("exit: returned from async branch");
    return;
  }

  DOUT("inetMutex locking");
  inetMutex_.lock();
  DOUT("inetMutex locked");

  Annotation annot; {
    annot.setTokenId(Token::TI_Public);
    annot.setUserId(server_->user().id());
    annot.setUserAlias(server_->user().name());
    annot.setUserAnonymous(server_->user().isAnonymous());
    annot.setLanguage(server_->user().language());
    annot.setCreateTime(QDateTime::currentMSecsSinceEpoch() / 1000);
    annot.setPos(annot.createTime());
    if (text.size() <= Traits::MAX_ANNOT_LENGTH)
      annot.setText(text);
    else {
      annot.setText(text.mid(0, Traits::MAX_ANNOT_LENGTH));
      warn(TR(T_WARNING_LONG_STRING_TRUNCATED) + ": " + annot.text());
    }
  }

  qint64 id = server_->submitLiveAnnotation(annot);

  if (id)
    annot.setId(id);
  else
    warn(TR(T_ERROR_SUBMIT_ANNOTATION) + ": " + text);

  //annotationBrowser_->addAnnotation(annot);
  //annotationView_->addAndShowAnnotation(annot);
  //dataManager_->token().annotCount()++;
  //dataManager_->invalidateToken();
  if (id && annot.userId() == User::UI_Guest)
    emit showAnnotationOnceRequested(annot);
  else
    emit showAnnotationRequested(annot);

  DOUT("inetMutex unlocking");
  inetMutex_.unlock();
  DOUT("inetMutex unlocked");
  DOUT("exit");
}

void
MainWindow::submitText(const QString &text, bool async)
{
  DOUT("enter: async =" << async << ", text =" << text);
  if (disposed_) {
    DOUT("exit: returned from disposed branch");
    return;
  }
  if (!server_->isAuthorized() ||
      !dataManager_->token().hasDigest() && !dataManager_->token().hasSource() ||
      hub_->isMediaTokenMode() && !player_->hasMedia()
#ifdef USE_MODE_SIGNAL
      || hub_->isSignalTokenMode() && !messageHandler_->lastMessageHash().isValid()
#endif // USE_MODE_SIGNAL
     ) {

    showText(text, true); // isSigned = true
    DOUT("exit: returned from showText branch");
    return;
  }

  Q_ASSERT(!hub_->isLiveTokenMode());

  if (async) {
    log(tr("connecting server to submit annot ..."));
    QThreadPool::globalInstance()->start(new task_::submitText(text, this));
    DOUT("exit: returned from async branch");
    return;
  }

  DOUT("inetMutex locking");
  inetMutex_.lock();
  DOUT("inetMutex locked");
  Annotation annot; {
    annot.setTokenId(dataManager_->token().id());
    annot.setTokenDigest(dataManager_->token().digest());
    annot.setTokenPart(dataManager_->token().part());
    annot.setUserId(server_->user().id());
    annot.setUserAlias(server_->user().name());
    annot.setUserAnonymous(server_->user().isAnonymous());
    annot.setLanguage(server_->user().language());
    annot.setCreateTime(QDateTime::currentMSecsSinceEpoch() / 1000);
    switch (hub_->tokenMode()) {
    case SignalHub::MediaTokenMode:
      annot.setPos(
        !player_->hasMedia() ? 0 :
        tokenType_ == Token::TT_Picture ? 0 :
        player_->time()
      );
      break;
    case SignalHub::LiveTokenMode:
      Q_ASSERT(0);
      break;
    case SignalHub::SignalTokenMode:
  #ifdef USE_MODE_SIGNAL
      annot.setPos(messageHandler_->lastMessageHash().hash);
      annot.setPosType(messageHandler_->lastMessageHash().count);
  #endif // USE_MODE_SIGNAL
      break;
    }

    if (text.size() <= Traits::MAX_ANNOT_LENGTH)
      annot.setText(text);
    else {
      annot.setText(text.mid(0, Traits::MAX_ANNOT_LENGTH));
      warn(TR(T_WARNING_LONG_STRING_TRUNCATED) + ": " + annot.text());
    }
  }

  qint64 id = dataServer_->submitAnnotation(annot);
  if (id)
    annot.setId(id);
  //else
  //  warn(TR(T_ERROR_SUBMIT_ANNOTATION) + ": " + text);

  //annotationBrowser_->addAnnotation(annot);
  //annotationView_->addAndShowAnnotation(annot);
  dataManager_->token().annotCount()++;
  dataManager_->invalidateToken();
  emit addAndShowAnnotationRequested(annot);

  DOUT("inetMutex unlocking");
  inetMutex_.unlock();
  DOUT("inetMutex unlocked");
  DOUT("exit");
}

void
MainWindow::eval(const QString &input)
{
  QString text = input.trimmed();
  if (text.isEmpty())
    return;

  QString dummy;
  QStringList tags;
  boost::tie(dummy, tags) = ANNOT_PARSE_CODE(text);

  foreach (const QString &tag, tags)
    switch (qHash(tag)) {
    case AnnotCloud::H_Chat: chat(text); return;
    case AnnotCloud::H_Play: play(); return;

    case AnnotCloud::H_Quit:
    case AnnotCloud::H_Exit:
    case AnnotCloud::H_Close:
      close();
      return;
    }

  if (hub_->isSignalTokenMode() && !hub_->isStopped() ||
      player_->hasMedia() && !player_->isStopped())
    submitText(text);
  else if (hub_->isLiveTokenMode() && server_->isConnected() && server_->isAuthorized())
    submitLiveText(text);
  else
    chat(text);
}

void
MainWindow::chat(const QString &input, bool async)
{
  DOUT("enter: async =" << async << ", text =" << input);
  if (disposed_) {
    DOUT("exit: returned from disposed branch");
    return;
  }
  QString text = input.trimmed();
  if (text.isEmpty()) {
    DOUT("exit: empty text skipped");
    return;
  }

  if (async) {
    //log(tr("connecting server to submit chat text ..."));
    QThreadPool::globalInstance()->start(new task_::chat(input, this));
    DOUT("exit: returned from async branch");
    return;
  }

  DOUT("inetMutex locking");
  inetMutex_.lock();
  DOUT("inetMutex locked");

  emit showTextRequested(CORE_CMD_COLOR_BLUE " " + text);
  emit said(text, "blue");

#ifdef USE_MODULE_DOLL
  Q_ASSERT(doll_);
  doll_->chat(text);
#else
  if (server_->isConnected())
    emit responded(server_->chat(text));
#endif // USE_MODULE_DOLL

  DOUT("inetMutex unlocking");
  inetMutex_.unlock();
  DOUT("inetMutex unlocked");
  DOUT("exit");
}

void
MainWindow::respond(const QString &text)
{
  DOUT("enter: text =" << text);
  showText(CORE_CMD_COLOR_PURPLE " " + text);
  say(text, "purple");
  DOUT("exit");
}

void
MainWindow::say(const QString &text, const QString &color)
{
  DOUT("enter: color =" << color << ", text =" << text);
  if (text.isEmpty()) {
    DOUT("exit: empty text");
    return;
  }

  if (color.isEmpty())
    log(text);
  else
    log(html_style(text, "color:" + color));
  DOUT("exit");
}

void
MainWindow::log(const QString &text)
{ Logger::log(text); }

void
MainWindow::warn(const QString &text)
{ Logger::warn(text); }

// - Events -

bool
MainWindow::event(QEvent *e)
{
  bool accept = true;
  switch (e->type()) {
  case QEvent::FileOpen: { // See: http://www.qtcentre.org/wiki/index.php?title=Opening_documents_in_the_Mac_OS_X_Finder
      QFileOpenEvent *fe = dynamic_cast<QFileOpenEvent*>(e);
      Q_ASSERT(fe);
      if (fe) {
        QString url = fe->file();
        QTimer::singleShot(0, new MainWindow_slot_::OpenSource(url, this), SLOT(openSource()));
      }
    } break;

  default: accept = Base::event(e);
  }

  return accept;
}

/*
void
MainWindow::setWindowDwmEnabled(bool t)
{
  if (UiStyle::isAeroAvailable())
    UiStyle::globalInstance()->setWindowDwmEnabled(this, t);
}
*/

void
MainWindow::setVisible(bool visible)
{
  //if (visible == isVisible())
  //  return;
  if (!isMinimized() && !isFullScreen()) { // invoked by showNormal or showMaximized
#ifdef USE_WIN_DWM
    if (UiStyle::isAeroAvailable())
      UiStyle::globalInstance()->setWindowDwmEnabled(this, true);
      //QTimer::singleShot(0,
      //  new MainWindow_slot_::SetWindowDwmEnabled(true, this), SLOT(setWindowDwmEnabled())
      //);
    else
#endif // USE_WIN_DWM
    UiStyle::globalInstance()->setWindowBackground(this, false); // persistent is false
  }

  Base::setVisible(visible);
}

void
MainWindow::mousePressEvent(QMouseEvent *event)
{
  //DOUT("enter");
  //if (contextMenu_->isVisible())
  //  contextMenu_->hide();
#ifdef Q_WS_MAC
  if (hub_->isEmbeddedPlayerMode()) {
    // Prevent auto hide osd player.
    // Since NSView::mouseMoved is not avaible, use mouseDown to prevent hiding instead.
    if (!hasVisiblePlayer()) {
      embeddedPlayer_->show();
      //osdDock_->raise();
      osdWindow_->raise();
      osdWindow_->setFocus(Qt::MouseFocusReason);
    } else if (embeddedPlayer_->isVisible())
      embeddedPlayer_->resetAutoHideTimeout();
      //QTimer::singleShot(0, embeddedPlayer_, SLOT(resetAutoHideTimeout()));

    // Alternative to windows hook:
    //if (isPlaying())
    //  pause();
  }
#endif // Q_WS_MAC

  if (event)
    switch (event->button()) {
    case Qt::LeftButton:
#ifdef Q_WS_MAC
      if (videoView_->isViewVisible())
        videoView_->setViewMousePressPos(event->globalPos());
#endif // Q_WS_MAC
      if (!isFullScreen() && dragPos_ == BAD_POS) {
        dragPos_ = event->globalPos() - frameGeometry().topLeft();
        event->accept();
      } break;

    case Qt::MiddleButton:
      hub_->toggleMiniPlayerMode();
      event->accept();
      break;

    case Qt::RightButton:
#ifndef Q_WS_MAC
      if (!contextMenu_->isVisible()) {
        QContextMenuEvent cm(QContextMenuEvent::Mouse, event->pos(), event->globalPos(), event->modifiers());
        QCoreApplication::sendEvent(this, &cm);
        event->accept();
      }
#endif // !Q_WS_MAC
      break;

    default: break;
    }

  Base::mousePressEvent(event);
  //DOUT("exit");
}

void
MainWindow::mouseReleaseEvent(QMouseEvent *event)
{
  //DOUT("enter");
  dragPos_ = BAD_POS;

#ifdef Q_WS_MAC
  if (event && event->button() == Qt::LeftButton && // event->buttons() is always zero, qt4 bug?
      videoView_->isViewVisible())
    videoView_->setViewMouseReleasePos(event->globalPos());
#endif // Q_WS_MAC

  if (event)
    event->accept();

  Base::mouseReleaseEvent(event);
  //DOUT("exit");
}

bool
MainWindow::isGlobalPosNearEmbeddedPlayer(const QPoint &pos) const
{
  QRect r = annotationView_->globalRect();
  return pos.x() < 6 || pos.x() > osdWindow_->width() - 6 ||  // Near the borders
         pos.y() < 6 || pos.y() > osdWindow_->height() - 6 ||
  (embeddedPlayer_->isOnTop() ? ( // Near the top
         pos.x() < r.right() + 6 && pos.x() > r.left() - 6 &&
         pos.y() < r.bottom() - r.height() / 4 * 3 + 6 && pos.y() > r.top() - 6
  ) : ( // Near the bottom
         pos.x() < r.right() + 6 && pos.x() > r.left() - 6 &&
         pos.y() < r.bottom() + 6 && pos.y() > r.top() + r.height() / 4 * 3 - 6
  ));
}

void
MainWindow::mouseMoveEvent(QMouseEvent *event)
{
  //DOUT("enter");

  // Prevent auto hide osd player.
  if (hub_->isEmbeddedPlayerMode())
    if (!event || isGlobalPosNearEmbeddedPlayer(event->globalPos())) {
      if (!hasVisiblePlayer())
        embeddedPlayer_->show();
      else if (embeddedPlayer_->isVisible())
        embeddedPlayer_->resetAutoHideTimeout();
        //QTimer::singleShot(0, embeddedPlayer_, SLOT(resetAutoHideTimeout()));
    }

#ifdef Q_WS_MAC
   if (event && videoView_->isViewVisible())
     videoView_->setViewMouseMovePos(event->globalPos());
#endif // Q_WS_MAC

  // Move the main window
  if (event && event->buttons() & Qt::LeftButton &&
      dragPos_ != BAD_POS && !isFullScreen()) {
    QPoint globalPos = event->globalPos();
    // FIXME: not working
    //QRect globalRect(globalPos, size());
    //if (!isRectOutOfScreen(globalRect))
      move(globalPos - dragPos_);

    annotationView_->invalidatePos();
//#ifdef Q_WS_MAC // FIXME: solve the postion tracking problem....
//  QTimer::singleShot(200, annotationView_, SLOT(invalidatePos()));
//#endif // Q_WS_MAC

    event->accept();
  }

  Base::mouseMoveEvent(event);
  //DOUT("exit");
}

void
MainWindow::mouseDoubleClickEvent(QMouseEvent *event)
{
  //DOUT("enter");
  if (event)
    switch (event->button()) {
    case Qt::LeftButton: hub_->toggleFullScreenWindowMode(); event->accept(); break;
    default: break;
    }

  Base::mouseDoubleClickEvent(event);
  //DOUT("exit");
}

void
MainWindow::wheelEvent(QWheelEvent *event)
{
  //DOUT("enter");
  if (event && event->orientation() == Qt::Vertical) {
    qreal vol = hub_->volume();
    vol += event->delta() / (8 * 360.0);
    vol = (vol > 1)? 1 : (vol < 0)? 0 : vol;
    hub_->setVolume(vol);
    event->accept();
  }

  Base::wheelEvent(event);
  //DOUT("exit");
}

void
MainWindow::contextMenuEvent(QContextMenuEvent *event)
{
  //DOUT("enter");
  if (event) {
    invalidateContextMenu();
    contextMenu_->popup(event->globalPos());
    event->accept(); }

  //Base::contextMenuEvent(event);
  //DOUT("exit");
}

void
MainWindow::invalidateAnnotationSubtitleMenu()
{
  toggleSubtitleAnnotationVisibleAct_->setChecked(annotationView_->isSubtitleVisible());
  toggleNonSubtitleAnnotationVisibleAct_->setChecked(annotationView_->isNonSubtitleVisible());
  toggleSubtitleOnTopAct_->setChecked(isSubtitleOnTop());

  bool t = toggleSubtitleAnnotationVisibleAct_->isChecked();
  toggleSubtitleOnTopAct_->setEnabled(t);
  subtitleStyleMenu_->setEnabled(t);
}

void
MainWindow::invalidateContextMenu()
{
  //DOUT("enter");
  contextMenu_->clear();
  if (!contextMenuActions_.isEmpty()) {
    foreach (QAction *a, contextMenuActions_)
      if (a)
        a->deleteLater();
    contextMenuActions_.clear();
  }

  bool online = server_->isConnected() && server_->isAuthorized();

  // Live mode
  if (online &&
      (hub_->isLiveTokenMode() || hub_->isStopped() && player_->isStopped())) {
    toggleLiveModeAct_->setChecked(hub_->isLiveTokenMode());
    contextMenu_->addAction(toggleLiveModeAct_);

    //toggleLiveDialogVisibleAct_->setChecked(liveDialog_->isVisible());
    //contextMenu_->addAction(toggleLiveDialogVisibleAct_);

    contextMenu_->addSeparator();
  }

  if (!currentUrl().isEmpty()) {
    contextMenu_->addAction(openInWebBrowserAct_);
    contextMenu_->addAction(downloadCurrentUrlAct_);
    contextMenu_->addSeparator();
  }

  // Open
  {
    //contextMenu_->addSeparator();

    //contextMenu_->addAction(openAct_);
    contextMenu_->addAction(openFileAct_);
    contextMenu_->addAction(openUrlAct_);
    contextMenu_->addAction(openDeviceAct_);

#ifdef USE_WIN_PICKER
    toggleWindowPickDialogVisibleAct_->setChecked(windowPickDialog_ && windowPickDialog_->isVisible());
    contextMenu_->addAction(toggleWindowPickDialogVisibleAct_);
#endif // USE_WIN_PICKER

    openMenu_->clear(); {
      if (hub_->isMediaTokenMode() && player_->hasMedia()) {
        openMenu_->addAction(openSubtitleAct_);
        openMenu_->addSeparator();
      }
      openMenu_->addAction(openVideoDeviceAct_);
#ifdef Q_WS_WIN // TODO add support for Mac/Linux
      openMenu_->addAction(openAudioDeviceAct_);
#endif // Q_WS_WIN
      //openMenu_->addAction(openFileAct_);
    }
    contextMenu_->addMenu(openMenu_);

    if (browsedFiles_.size() > 1)
      contextMenu_->addMenu(browseMenu_);

    // Recent
    invalidateRecent();
    if (!recentFiles_.isEmpty())
      contextMenu_->addMenu(recentMenu_);

    // - Playlist
    //invalidatePlaylist();
    if (playlist_.size() > 1 && hub_->isMediaTokenMode())
      contextMenu_->addMenu(playlistMenu_);

    // Tracks
    if (hub_->isMediaTokenMode() && player_->hasMedia() &&
        player_->trackCount() > 1)
      //invalidateTrackMenu();
      contextMenu_->addMenu(trackMenu_);

    // DVD sections
    if (hub_->isMediaTokenMode() && player_->hasMedia() &&
        player_->titleCount() > 1) {

      sectionMenu_->clear();

      if (player_->titleId() > 0)
        sectionMenu_->addAction(previousSectionAct_);
      if (player_->titleId() < player_->titleCount() - 1)
        sectionMenu_->addAction(nextSectionAct_);
      sectionMenu_->addSeparator();

      for (int tid = 0; tid < player_->titleCount(); tid++) {
        QString text = TR(T_SECTION) + " " + QString::number(tid+1);
        BOOST_AUTO(a, new QtExt::ActionWithId(tid, text, sectionMenu_));
        contextMenuActions_.append(a);
        if (tid == player_->titleId()) {
#ifdef Q_WS_WIN
          a->setIcon(QIcon(RC_IMAGE_CURRENTSECTION));
#else
          a->setCheckable(true);
          a->setChecked(true);
#endif // Q_WS_WIN
        }
        connect(a, SIGNAL(triggeredWithId(int)), player_, SLOT(setTitleId(int)));
        sectionMenu_->addAction(a);
      }

      contextMenu_->addMenu(sectionMenu_);
    }

#ifdef USE_MODE_SIGNAL
    if (!player_->hasMedia() || player_->isStopped()) {
      contextMenu_->addSeparator();
#ifdef USE_WIN_PICKER
      toggleProcessPickDialogVisibleAct_->setChecked(processPickDialog_ && processPickDialog_->isVisible());
      contextMenu_->addAction(toggleProcessPickDialogVisibleAct_);
#endif // USE_WIN_PICKER

       toggleSignalViewVisibleAct_->setChecked(signalView_->isVisible());
       contextMenu_->addAction(toggleSignalViewVisibleAct_ );

       if (hub_->isSignalTokenMode()) {
         toggleRecentMessageViewVisibleAct_->setChecked(recentMessageView_->isVisible());
         contextMenu_->addAction(toggleRecentMessageViewVisibleAct_ );
       }
     }
#endif // USE_MODE_SIGNAL

    {
      contextMenu_->addSeparator();
      toggleSiteAccountViewVisibleAct_->setChecked(siteAccountView_ && siteAccountView_->isVisible());
      int count = annotationFilter_->annotationCountHint();
      QString text = count <= 0 ? TR(T_MENUTEXT_SITEACCOUNT) :
        QString("%1 (%2)").arg(TR(T_MENUTEXT_SITEACCOUNT))
                          .arg(QString::number(count));
      toggleSiteAccountViewVisibleAct_->setText(text);
      contextMenu_->addAction(toggleSiteAccountViewVisibleAct_);

      toggleDownloadDialogVisibleAct_->setChecked(backlogDialog_ && backlogDialog_->isVisible());
      contextMenu_->addAction(toggleDownloadDialogVisibleAct_);
    }

    if (hub_->isMediaTokenMode() && player_->hasMedia()) {
      contextMenu_->addAction(openAnnotationUrlAct_);

      // Subtitle menu

      subtitleMenu_->clear();
      if (player_->hasSubtitles()) {
        bool t = player_->isSubtitleVisible();
        toggleSubtitleVisibleAct_->setChecked(t);

        QStringList l = player_->subtitleDescriptions();
        int id = 0;
        foreach (QString subtitle, l) {
          if (subtitle.isEmpty())
            subtitle = TR(T_SUBTITLE) + " " + QString::number(id+1);
          else
            subtitle = QString::number(id+1) + ". " + subtitle;
          BOOST_AUTO(a, new QtExt::ActionWithId(id, subtitle, subtitleMenu_));
          contextMenuActions_.append(a);
          if (id == player_->subtitleId()) {
#ifdef Q_WS_WIN
            a->setIcon(QIcon(RC_IMAGE_CURRENTSUBTITLE));
#else
            a->setCheckable(true);
            a->setChecked(true);
#endif // Q_WS_WIN
          }
          a->setEnabled(t);
          if (t)
            connect(a, SIGNAL(triggeredWithId(int)), player_, SLOT(setSubtitleId(int)));
          subtitleMenu_->addAction(a);
          id++;
        }

        subtitleMenu_->addSeparator();
        subtitleMenu_->addAction(toggleSubtitleVisibleAct_);
      }
      subtitleMenu_->addAction(openSubtitleAct_);
      contextMenu_->addMenu(subtitleMenu_);

      // Audio track menu
      if (player_->hasAudioTracks()) {
        audioTrackMenu_->clear();

        QStringList l = player_->audioTrackDescriptions();
        int id = 0;
        foreach (QString name, l) {
          if (name.isEmpty())
            name = TR(T_AUDIOTRACK) + " " + QString::number(id+1);
          else
            name = QString::number(id+1) + ". " + name;
          BOOST_AUTO(a, new QtExt::ActionWithId(id, name, audioTrackMenu_));
          contextMenuActions_.append(a);
          if (id == player_->audioTrackId()) {
#ifdef Q_WS_WIN
            a->setIcon(QIcon(RC_IMAGE_CURRENTTRACK));
#else
            a->setCheckable(true);
            a->setChecked(true);
#endif // Q_WS_WIN
          }
          connect(a, SIGNAL(triggeredWithId(int)), player_, SLOT(setAudioTrackId(int)));
          audioTrackMenu_->addAction(a);
          id++;
        }

        contextMenu_->addMenu(audioTrackMenu_);
      }
    }
  }

  // Basic control
  contextMenu_->addSeparator();
  if (!hub_->isMediaTokenMode() || player_->hasMedia()) {
    contextMenu_->addAction(isPlaying() ? pauseAct_ : playAct_);

    playMenu_->clear();

    // jichi 11/18/2011: Removed from play menu only in order to save space ....
    //if (hub_->isMediaTokenMode())
    //  playMenu_->addAction(nextFrameAct_);

    if (!hub_->isMediaTokenMode() && !hub_->isStopped() ||
        hub_->isMediaTokenMode() && !player_->isStopped())
    playMenu_->addAction(stopAct_);

    if (hub_->isMediaTokenMode()) {
      playMenu_->addAction(replayAct_);
      playMenu_->addAction(previousAct_);
      playMenu_->addAction(nextAct_);
      playMenu_->addAction(toggleAutoPlayNextAct_);
    }

    contextMenu_->addMenu(playMenu_);
  }
  if (hub_->isMediaTokenMode() && player_->hasMedia()) {
    contextMenu_->addMenu(backwardMenu_);
    contextMenu_->addMenu(forwardMenu_);

    toggleSeekDialogVisibleAct_->setChecked(seekDialog_ && seekDialog_->isVisible());
    contextMenu_->addAction(toggleSeekDialogVisibleAct_);
  }
  if (!hub_->isMediaTokenMode() || !player_->isStopped())
    contextMenu_->addAction(snapshotAct_);

  // Modes
  {
    contextMenu_->addSeparator();

    toggleFullScreenModeAct_->setChecked(hub_->isFullScreenWindowMode());
    contextMenu_->addAction(toggleFullScreenModeAct_);

    toggleMiniModeAct_->setChecked(hub_->isMiniPlayerMode());
    contextMenu_->addAction(toggleMiniModeAct_);

    toggleEmbeddedModeAct_->setChecked(hub_->isEmbeddedPlayerMode());
    contextMenu_->addAction(toggleEmbeddedModeAct_);
    if (hub_->isEmbeddedPlayerMode()) {
      toggleEmbeddedPlayerOnTopAct_->setChecked(embeddedPlayer_->isOnTop());
      contextMenu_->addAction(toggleEmbeddedPlayerOnTopAct_);
    }
  }

  // Annotation filter
  {
    contextMenu_->addSeparator();

    toggleAnnotationFilterEnabledAct_->setChecked(annotationFilter_->isEnabled());
    contextMenu_->addAction(toggleAnnotationFilterEnabledAct_);
    if (annotationFilter_->isEnabled()) {
      toggleBlacklistViewVisibleAct_->setChecked(toggleAnnotationFilterEnabledAct_ && toggleAnnotationFilterEnabledAct_->isVisible());
      contextMenu_->addAction(toggleBlacklistViewVisibleAct_);

      toggleAnnotationCountDialogVisibleAct_->setChecked(annotationCountDialog_ && annotationCountDialog_->isVisible());
      contextMenu_->addAction(toggleAnnotationCountDialogVisibleAct_ );

      contextMenu_->addMenu(annotationLanguageMenu_);
    }
  }

  // Annotations
  {
    contextMenu_->addSeparator();

    toggleAnnotationVisibleAct_->setChecked(annotationView_ && annotationView_->isVisible());
    contextMenu_->addAction(toggleAnnotationVisibleAct_ );

    toggleBacklogDialogVisibleAct_->setChecked(backlogDialog_ && backlogDialog_->isVisible());
    contextMenu_->addAction(toggleBacklogDialogVisibleAct_);

    bool t = toggleAnnotationVisibleAct_->isChecked();

    toggleBlacklistViewVisibleAct_->setEnabled(t);
    toggleBlacklistViewVisibleAct_->setChecked(blacklistView_ && blacklistView_->isVisible());

    if (t)
      invalidateAnnotationEffectMenu();
    annotationEffectMenu_->setEnabled(t);
    contextMenu_->addMenu(annotationEffectMenu_);

    if (t)
      invalidateAnnotationSubtitleMenu();
    annotationSubtitleMenu_->setEnabled(t);
    contextMenu_->addMenu(annotationSubtitleMenu_);

    if (hub_->isSignalTokenMode()) {
      toggleTranslateAct_->setEnabled(t && online);
      contextMenu_->addAction(toggleTranslateAct_);
    }

    toggleAnnotationEditorVisibleAct_->setChecked(annotationEditor_ && annotationEditor_->isVisible());
    contextMenu_->addAction(toggleAnnotationEditorVisibleAct_ );

    if (!hub_->isMediaTokenMode() || player_->hasMedia()) {
      toggleAnnotationBrowserVisibleAct_->setChecked(annotationBrowser_->isVisible());
      contextMenu_->addAction(toggleAnnotationBrowserVisibleAct_ );

      toggleTokenViewVisibleAct_->setChecked(tokenView_->isVisible());
      contextMenu_->addAction(toggleTokenViewVisibleAct_ );
    }

    //if (ALPHA) if (commentView_ && commentView_->tokenId()) {
    //  toggleCommentViewVisibleAct_->setChecked(commentView_ && commentView_->isVisible());
    //  contextMenu_->addAction(toggleCommentViewVisibleAct_ );
    //}
  }

  // User
  if (!server_->isAuthorized()) {
    contextMenu_->addSeparator();
    contextMenu_->addAction(loginAct_);
  }

//  if (ALPHA)
//  if (player_->hasMedia()
//#ifdef USE_MODE_SIGNAL
//      && hub_->isMediaTokenMode()
//#endif // USE_MODE_SIGNAL
//      ) {
//    // Sync mode
//    contextMenu_->addSeparator();
//
//    toggleSyncModeAct_->setChecked(hub_->isSyncPlayMode());
//    contextMenu_->addAction(toggleSyncModeAct_);
//
//    toggleSyncDialogVisibleAct_->setChecked(syncDialog_ && syncDialog_->isVisible());
//    contextMenu_->addAction(toggleSyncDialogVisibleAct_);
//  }

  // App
  {
    contextMenu_->addSeparator();

    // Theme
    if (themeMenu_) {
      contextMenu_->addMenu(themeMenu_);

      UiStyle::Theme t = UiStyle::globalInstance()->theme();
      setThemeToDefaultAct_->setChecked(t == UiStyle::DefaultTheme);
      setThemeToRandomAct_->setChecked(t == UiStyle::RandomTheme);
      setThemeToDarkAct_->setChecked(t == UiStyle::DarkTheme);
      setThemeToBlackAct_->setChecked(t == UiStyle::BlackTheme);
      setThemeToBlueAct_->setChecked(t == UiStyle::BlueTheme);
      setThemeToBrownAct_->setChecked(t == UiStyle::BrownTheme);
      setThemeToCyanAct_->setChecked(t == UiStyle::CyanTheme);
      setThemeToGrayAct_->setChecked(t == UiStyle::GrayTheme);
      setThemeToGreenAct_->setChecked(t == UiStyle::GreenTheme);
      setThemeToPinkAct_->setChecked(t == UiStyle::PinkTheme);
      setThemeToPurpleAct_->setChecked(t == UiStyle::PurpleTheme);
      setThemeToRedAct_->setChecked(t == UiStyle::RedTheme);
      setThemeToWhiteAct_->setChecked(t == UiStyle::WhiteTheme);
      setThemeToYellowAct_->setChecked(t == UiStyle::YellowTheme);
    }

    // Language
    if (player_->isStopped() && hub_->isStopped() ||
        hub_->isLiveTokenMode()) {
      contextMenu_->addMenu(appLanguageMenu_);

      //int l = TranslatorManager::globalInstance()->language();
      int l = Settings::globalInstance()->language();
      setAppLanguageToJapaneseAct_->setChecked(l == QLocale::Japanese);
      //setAppLanguageToTraditionalChineseAct_->setChecked(l == TranslatorManager::TraditionalChinese);
      setAppLanguageToSimplifiedChineseAct_->setChecked(l == QLocale::Chinese);

      switch (l) {
      case QLocale::Japanese: case QLocale::Taiwan: case QLocale::Chinese:
        setAppLanguageToEnglishAct_->setChecked(false);
        break;
      case QLocale::English: default:
        setAppLanguageToEnglishAct_->setChecked(true);
      }
    }

  }

  // Help
  {
    contextMenu_->addSeparator();

    //contextMenu_->addAction(showMinimizedAct_);
    contextMenu_->addAction(checkInternetConnectionAct_);
    //if (hub_->isStopped() && player_->isStopped())
    //  contextMenu_->addAction(deleteCachesAct_);

    toggleWindowOnTopAct_->setChecked(isWindowOnTop());
    contextMenu_->addAction(toggleWindowOnTopAct_);

#ifdef Q_OS_LINUX
    toggleMenuBarVisibleAct_->setChecked(menuBar()->isVisible());
    contextMenu_->addAction(toggleMenuBarVisibleAct_);
#endif // Q_OS_LINUX

    contextMenu_->addMenu(helpContextMenu_);

    contextMenu_->addAction(quitAct_);
  }
  //DOUT("exit");
}

void
MainWindow::invalidateUserMenu()
{
  //userMenu_->clear();
  if (!server_->isAuthorized())
    return;

  // Menu

  if (userMenu_->isEmpty()) {
    userMenu_->addAction(toggleUserAnonymousAct_);
    userMenu_->addMenu(userLanguageMenu_);

    userMenu_->addSeparator();
    userMenu_->addAction(toggleUserViewVisibleAct_);
    userMenu_->addAction(logoutAct_);
  }

  toggleUserViewVisibleAct_->setChecked(userView_ && userView_->isVisible());

  //if (ALPHA) {
  //  if (server_->isConnected())
  //    userMenu_->addAction(disconnectAct_);
  //  else
  //    userMenu_->addAction(connectAct_);
  //}

  // Actions

  toggleUserAnonymousAct_->setChecked(server_->user().isAnonymous());

  {
    setUserLanguageToEnglishAct_->setChecked(server_->user().language() == Traits::English);
    setUserLanguageToJapaneseAct_->setChecked(server_->user().language() == Traits::Japanese);
    setUserLanguageToChineseAct_->setChecked(server_->user().language() == Traits::Chinese);
    setUserLanguageToKoreanAct_->setChecked(server_->user().language() == Traits::Korean);
    setUserLanguageToUnknownAct_->setChecked(server_->user().language() == Traits::UnknownLanguage);
  }
}

void
MainWindow::invalidateTrackMenu()
{
  trackMenu_->clear();
  if (!player_->hasMedia() || !player_->hasPlaylist())
    return;

  foreach (Player::MediaInfo mi, player_->playlist()) {
    QString text = QString("%1. %2").arg(QString::number(mi.track + 1)).arg(mi.title);
    BOOST_AUTO(a, new QtExt::ActionWithId(mi.track, text, trackMenu_));
    a->setToolTip(mi.path);
    if (mi.track == player_->trackNumber()) {
#ifdef Q_WS_X11
      a->setCheckable(true);
      a->setChecked(true);
#else
      a->setIcon(QIcon(RC_IMAGE_CURRENTTRACK));
#endif // Q_WS_X11
    }
    connect(a, SIGNAL(triggeredWithId(int)), player_, SLOT(setTrackNumber(int)));
    trackMenu_->addAction(a);
  }

  trackMenu_->addSeparator();

  trackMenu_->addAction(QIcon(RC_IMAGE_PREVIOUS), TR(T_PREVIOUS), player_, SLOT(previousTrack()));
  trackMenu_->addAction(QIcon(RC_IMAGE_NEXT), TR(T_NEXT), player_, SLOT(nextTrack()));
}

// - Move and resize events -

void
MainWindow::moveEvent(QMoveEvent *event)
{
  Q_UNUSED(event);
  if (annotationView_->isVisible()) {
    annotationView_->invalidatePos();
//#ifdef Q_WS_MAC // FIXME: solve the postion tracking problem....
//    QTimer::singleShot(200, annotationView_, SLOT(invalidatePos()));
//#endif // Q_WS_MAC
  }

  Base::moveEvent(event);
}

void
MainWindow::resizeEvent(QResizeEvent *event)
{
  Q_UNUSED(event);
  //if (isMaximized()) {
  //  setFullScreenPlayMode();
  //  return;
  //}
  if (annotationView_->isVisible()) {
    annotationView_->invalidateSize();
    globalOsdConsole_->resize(annotationView_->size());
  }

  Base::resizeEvent(event);
}

void
MainWindow::changeEvent(QEvent *event)
{
  Q_ASSERT(event);
  Base::changeEvent(event);

  switch (event->type()) {
  case QEvent::WindowStateChange:
#ifdef Q_WS_X11
    if (windowState() == Qt::WindowFullScreen)
      QtX::setWindowInputShape(osdWindow_->winId(), embeddedPlayer_->pos(), embeddedPlayer_->rect());
    else
      QtX::zeroWindowInputShape(osdWindow_->winId());
#endif // Q_WS_X11
    break;

  default: break;
  }
}

// - Keyboard events -

void
MainWindow::keyPressEvent(QKeyEvent *event)
{
  switch (event->key()) {

    //if (!hasVisiblePlayer() || !visiblePlayer()->lineEdit()->hasFocus())
  case Qt::Key_Escape:  hub_->toggleMiniPlayerMode(); break;
  case Qt::Key_Return:  hub_->toggleFullScreenWindowMode(); break;
  //case Qt::Key_Backspace: stop(); break;
  case Qt::Key_Space:   playPause(); break;

  case Qt::Key_Up:      volumeUp(); break;
  case Qt::Key_Down:    volumeDown(); break;

  case Qt::Key_Left:
    if (event->modifiers() == Qt::NoModifier) backward();
    else backward(G_BACKWARD_INTERVAL * 3);
    break;

  case Qt::Key_Right:
    if (event->modifiers() == Qt::NoModifier) forward();
    else forward(G_FORWARD_INTERVAL * 3);
    break;

  default:
    if (hub_->isFullScreenWindowMode()) { // Osd mode
      if (!hasVisiblePlayer())
        embeddedPlayer_->show();
      else if (embeddedPlayer_->isVisible())
        embeddedPlayer_->resetAutoHideTimeout();
        //QTimer::singleShot(0, embeddedPlayer_, SLOT(resetAutoHideTimeout()));
    }
    if (hasVisiblePlayer()
        && !visiblePlayer()->inputComboBox()->hasFocus()
        && !visiblePlayer()->prefixComboBox()->hasFocus()) {
#ifdef Q_WS_WIN
      QtWin::setFocus(visiblePlayer()->inputComboBox());
#else
      visiblePlayer()->inputComboBox()->setFocus();
#endif // Q_WS_WIN
    }
    Base::keyPressEvent(event);
  }
}

// - Drag and drop events -

bool
MainWindow::isMimeDataSupported(const QMimeData *mime)
{ return mime && mime->hasUrls(); }

void
MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
  DOUT("enter");
  Q_ASSERT(event);
  if (isMimeDataSupported(event->mimeData()))
    event->acceptProposedAction();
  Base::dragEnterEvent(event);
  DOUT("exit");
}

void
MainWindow::dragMoveEvent(QDragMoveEvent *event)
{
  //DOUT("enter");
  Q_ASSERT(event);
  //if (isMimeDataSupported(event->mimeData()))
    event->acceptProposedAction();
  Base::dragMoveEvent(event);
  //DOUT("exit");
}

void
MainWindow::dropEvent(QDropEvent *event)
{
  DOUT("enter");
  Q_ASSERT(event);
  if (isMimeDataSupported(event->mimeData())) {
    setFocus();
    event->acceptProposedAction();
    openMimeData(event->mimeData());
  }
  Base::dropEvent(event);
  DOUT("exit");
}

void
MainWindow::dragLeaveEvent(QDragLeaveEvent *event)
{
  DOUT("enter");
  event->accept();
  Base::dragLeaveEvent(event);
  DOUT("exit");
}

// - Close -
void
MainWindow::closeEvent(QCloseEvent *event)
{
  DOUT("enter");
  closeEventEnter();
  QTimer::singleShot(0,
                     new MainWindow_slot_::CloseEventLeave(event, this),
                     SLOT(closeEventLeave()));
  DOUT("exit");
}

void
MainWindow::closeEventEnter()
{
  DOUT("enter");
  //respond(tr("yin: ...")); // crash on mac
  //log(tr("closing application ..."));

  if (player_->hasMedia()) {
    rememberPlayPos();
    rememberSubtitle();
    rememberAudioTrack();
  }
  player_->dispose();

  hide();

  osdWindow_->hide();
  annotationView_->hide();
  globalOsdConsole_->hide();

  if (tray_)
    tray_->hide();

#ifdef USE_MODE_SIGNAL
  QTH->clear();
#endif // USE_MODE_SIGNAL

  hub_->stop();

  dataServer_->dispose();

  if (downloadDialog_)
    downloadDialog_->stopAll();

#ifdef USE_WIN_PICKER
  if (PICKER->isActive())
    PICKER->stop();
#endif // USE_WIN_PICKER
#ifdef USE_WIN_HOOK
  if (HOOK->isActive())
    HOOK->stop();
#endif // USE_WIN_HOOK

  FlvCodec::globalInstance()->stop();
  DOUT("exit");
}

void
MainWindow::closeEventLeave(QCloseEvent *event)
{
  DOUT("enter");
  // Save settings
  Settings *settings = Settings::globalInstance();

  //invalidateRecent();
  settings->setRecentFiles(recentFiles_);

  settings->setRecentPath(recentPath_);

  settings->setThemeId(UiStyle::globalInstance()->theme());

  settings->setThemeId(UiStyle::globalInstance()->theme());

  settings->setMenuBarVisible(menuBar()->isVisible());

  settings->setAnnotationFilterEnabled(annotationFilter_->isEnabled());
  settings->setBlockedKeywords(annotationFilter_->blockedTexts());
  settings->setBlockedUserNames(annotationFilter_->blockedUserAliases());
  settings->setAnnotationLanguages(annotationFilter_->languages());
  //settings->setAnnotationCountHint(annotationFilter_->annotationCountHint());

  settings->setSubtitleColor(subtitleColor());

  settings->setAnnotationEffect(annotationView_->renderHint());

  // Disabled for saving closing time orz
  //if (server_->isConnected() && server_->isAuthorized())
  //  server_->logout();

  settings->setTranslateEnabled(isTranslateEnabled());

  settings->setEmbeddedPlayerOnTop(embeddedPlayer_->isOnTop());

  settings->setAutoPlayNext(isAutoPlayNext());

  settings->setLive(hub_->isLiveTokenMode());

  if (siteAccountView_) { // Update account settings iff it is modified
    settings->setNicovideoAccount(siteAccountView_->nicovideoAccount().username,
                                  siteAccountView_->nicovideoAccount().password);
    settings->setBilibiliAccount(siteAccountView_->bilibiliAccount().username,
                                 siteAccountView_->bilibiliAccount().password);
  }

  settings->setPlayPosHistory(playPosHistory_);
  settings->setSubtitleHistory(subtitleHistory_);
  settings->setAudioTrackHistory(audioTrackHistory_);

  //MediaStreamer::globalInstance()->stop();

  //{
  //  StreamService *ss = StreamService::globalInstance();
  //  ss->stop(10 * 1000); // 10 seconds
  //  if (ss->isActive())
  //    ss->terminateService();
  //}

  if (QThreadPool::globalInstance()->activeThreadCount()) {
#if QT_VERSION >= 0x040800
    // wait for at most 5 seconds ant kill all threads
    QThreadPool::globalInstance()->waitForDone(5000);
#else
    //DOUT("WARNING: killing active threads; will be fixed in Qt 4.8");
    QThreadPool::globalInstance()->waitForDone();
#endif  // QT_VERSION
  }

  //if (parentWidget())
  //  QTimer::singleShot(0, parentWidget(), SLOT(close()));

#ifdef Q_OS_WIN
  QTimer::singleShot(0, qApp, SLOT(quit())); // ensure quit app and clean up zombie threads
#endif // Q_OS_WIN

  Base::closeEvent(event);
  emit windowClosed();
  DOUT("exit");
}

// - Window on top -

bool
MainWindow::isWindowOnTop() const
{ return windowFlags() & Qt::WindowStaysOnTopHint; }

void
MainWindow::setWindowOnTop(bool t)
{
  if (t != isWindowOnTop()) {
//#ifdef USE_WIN_DWM
//    UiStyle::globalInstance()->setDwmEnabled(false);
//#endif // USE_WIN_DWM
    bool visible = isVisible();
    if (t)
      setWindowFlags(windowFlags() | Qt::WindowStaysOnTopHint);
    else
      setWindowFlags(windowFlags() & ~Qt::WindowStaysOnTopHint);

//#ifdef USE_WIN_QTH
//    QTH->setParentWinId(winId());
//#endif // USE_WIN_QTH

#ifdef USE_WIN_DWM
    UiStyle::globalInstance()->setDwmEnabled(true);
#endif // USE_WIN_DWM
    if (visible) {
      if (hub_->isFullScreenWindowMode())
        showFullScreen();
      else
        show();
    }

    // FIXME: make it work in Windows
    //osdWindow_->ensureStaysOnTop();

    if (t)
      log(tr("always on top enabled"));
    else
      log(tr("always on top disabled"));
  }

#ifdef Q_WS_WIN
  if (t) {
    if (!hub_->isMediaTokenMode() && annotationView_->trackedWindow() &&
        !QtWin::isWindowAboveWindow(winId(), annotationView_->trackedWindow()))
      QtWin::setTopWindow(winId());
    if (!windowStaysOnTopTimer_->isActive())
      windowStaysOnTopTimer_->start();
  } else {
    if (windowStaysOnTopTimer_->isActive())
      windowStaysOnTopTimer_->stop();
  }
#endif // Q_WS_WIN
}

// - Synchronize -

void
MainWindow::blessTokenWithId(qint64 id, bool async)
{
  DOUT("enter: async =" << async);
  if (disposed_) {
    DOUT("exit: returned from disposed branch");
    return;
  }
  if (!id) {
    warn(tr("invalid cast id"));
    DOUT("exit from invalid parameter branch");
    return;
  }

  if (!server_->isConnected() || !server_->isAuthorized()) {
    warn(tr("cannot perform cast when offline"));
    DOUT("exit from offline branch");
    return;
  }

  if (async) {
    log(tr("submit bless cast to token ..."));
    QThreadPool::globalInstance()->start(new task_::blessTokenWithId(id, this));
    DOUT("exit: returned from async branch");
    return;
  }

  DOUT("inetMutex locking");
  inetMutex_.lock();
  DOUT("inetMutex locked");

  bool ok = server_->blessTokenWithId(id);
  if (ok) {
    log(tr("token blessed"));
    if (dataManager_->token().id() == id) {
      dataManager_->token().blessedCount()++;
      //dataManager_->invalidateToken();
    }
  } else
    warn(tr("failed to bless token"));

  DOUT("inetMutex unlocking");
  inetMutex_.unlock();
  DOUT("inetMutex unlocked");
  DOUT("exit: async =" << async);
}

void
MainWindow::curseTokenWithId(qint64 id, bool async)
{
  DOUT("enter: async =" << async);
  if (!id) {
    warn(tr("invalid cast id"));
    DOUT("exit from invalid parameter branch");
    return;
  }

  if (!server_->isConnected() || !server_->isAuthorized()) {
    warn(tr("cannot perform cast when offline"));
    DOUT("exit from offline branch");
    return;
  }

  if (async) {
    log(tr("submit curse cast to token ..."));
    QThreadPool::globalInstance()->start(new task_::curseTokenWithId(id, this));
    DOUT("exit: returned from async branch");
    return;
  }

  DOUT("inetMutex locking");
  inetMutex_.lock();
  DOUT("inetMutex locked");

  bool ok = server_->curseTokenWithId(id);
  if (ok) {
    log(tr("token cursed"));
    if (dataManager_->token().id() == id) {
      dataManager_->token().cursedCount()++;
      //dataManager_->invalidateToken();
    }
  } else
    warn(tr("failed to curse token"));

  DOUT("inetMutex unlocking");
  inetMutex_.unlock();
  DOUT("inetMutex unlocked");
  DOUT("exit: async =" << async);
}

void
MainWindow::blessUserWithId(qint64 id, bool async)
{
  DOUT("enter: async =" << async);
  if (disposed_) {
    DOUT("exit: returned from disposed branch");
    return;
  }
  if (!id) {
    warn(tr("invalid cast id"));
    DOUT("exit from invalid parameter branch");
    return;
  }

  if (!server_->isConnected() || !server_->isAuthorized()) {
    warn(tr("cannot perform cast when offline"));
    DOUT("exit from offline branch");
    return;
  }
  if (id == server_->user().id()) {
    warn(tr("cannot perform cast to yourself"));
    DOUT("exit from self branch");
    return;
  }

  if (async) {
    log(tr("blessing user ..."));
    QThreadPool::globalInstance()->start(new task_::blessUserWithId(id, this));
    DOUT("exit: returned from async branch");
    return;
  }

  DOUT("inetMutex locking");
  inetMutex_.lock();
  DOUT("inetMutex locked");

  server_->blessUserWithId(id);
  log(tr("user blessed"));

  DOUT("inetMutex unlocking");
  inetMutex_.unlock();
  DOUT("inetMutex unlocked");
  DOUT("exit: async =" << async);
}

void
MainWindow::curseUserWithId(qint64 id, bool async)
{
  DOUT("enter: async =" << async);
  if (disposed_) {
    DOUT("exit: returned from disposed branch");
    return;
  }
  if (!id) {
    warn(tr("invalid cast id"));
    DOUT("exit from invalid parameter branch");
    return;
  }

  if (!server_->isConnected() || !server_->isAuthorized()) {
    warn(tr("cannot perform cast when offline"));
    DOUT("exit from offline branch");
    return;
  }
  if (id == server_->user().id()) {
    warn(tr("cannot perform cast to yourself"));
    DOUT("exit from self branch");
    return;
  }

  if (async) {
    log(tr("cursing user ..."));
    QThreadPool::globalInstance()->start(new task_::curseUserWithId(id, this));
    DOUT("exit: returned from async branch");
    return;
  }

  DOUT("inetMutex locking");
  inetMutex_.lock();
  DOUT("inetMutex locked");

  server_->curseUserWithId(id);
  log(tr("user cursed"));

  DOUT("inetMutex unlocking");
  inetMutex_.unlock();
  DOUT("inetMutex unlocked");
  DOUT("exit: async =" << async);
}

void
MainWindow::blockUserWithId(qint64 id, bool async)
{
  DOUT("enter: async =" << async);
  if (disposed_) {
    DOUT("exit: returned from disposed branch");
    return;
  }
  if (!id) {
    warn(tr("invalid cast id"));
    DOUT("exit from invalid parameter branch");
    return;
  }

  if (!server_->isConnected() || !server_->isAuthorized()) {
    warn(tr("cannot perform cast when offline"));
    DOUT("exit from offline branch");
    return;
  }
  if (id == server_->user().id()) {
    warn(tr("cannot perform cast to yourself"));
    DOUT("exit from self branch");
    return;
  }

  if (async) {
    log(tr("blocking user ..."));
    QThreadPool::globalInstance()->start(new task_::blockUserWithId(id, this));
    DOUT("exit: returned from async branch");
    return;
  }

  DOUT("inetMutex locking");
  inetMutex_.lock();
  DOUT("inetMutex locked");

  server_->blockUserWithId(id);
  log(tr("user blocked"));

  DOUT("inetMutex unlocking");
  inetMutex_.unlock();
  DOUT("inetMutex unlocked");
  DOUT("exit: async =" << async);
}

#define CAST(_cast, _annot) \
  void \
  MainWindow::_cast##_annot##WithId(qint64 id, bool async) \
  { \
    DOUT("enter: async =" << async); \
    if (disposed_) { \
      DOUT("exit: returned from disposed branch"); \
      return; \
    } \
    if (!id) { \
      warn(tr("invalid cast id")); \
      DOUT("exit from invalid parameter branch"); \
      return; \
    } \
\
    if (!server_->isConnected() || !server_->isAuthorized()) { \
      warn(tr("cannot perform cast when offline")); \
      DOUT("exit from offline branch"); \
      return; \
    } \
\
    if (async) { \
      log(tr("casting spell to target ...")); \
      QThreadPool::globalInstance()->start(new task_::blockUserWithId(id, this)); \
      DOUT("exit: returned from async branch"); \
      return; \
    } \
\
    DOUT("inetMutex locking"); \
    inetMutex_.lock(); \
    DOUT("inetMutex locked"); \
\
    server_->_cast##_annot##WithId(id); \
\
    DOUT("inetMutex unlocking"); \
    inetMutex_.unlock(); \
    DOUT("inetMutex unlocked"); \
    DOUT("exit: async =" << async); \
  }

 CAST(bless, Annotation)
 CAST(block, Annotation)
 CAST(curse, Annotation)
 CAST(bless, Alias)
 CAST(block, Alias)
 CAST(curse, Alias)
#undef CAST

void
MainWindow::logout(bool async)
{
  DOUT("enter: async =" << async);
  if (disposed_) {
    DOUT("exit: returned from disposed branch");
    return;
  }
  Q_UNUSED(async);
  //if (async) {
  //  log(tr("connecting server to logout ..."));
  //  QThreadPool::globalInstance()->start(new task_logout(this));
  //  DOUT("exit: returned from async branch");
  //  return;
  //}

  //DOUT("inetMutex locking");
  //inetMutex_.lock();
  //DOUT("inetMutex locked");

  annotationBrowser_->setUserId(0);
  server_->logout();

  //DOUT("inetMutex unlocking");
  //inetMutex_.unlock();
  //DOUT("inetMutex unlocked");

  emit userChanged(User());

  Settings *s = Settings::globalInstance();
  //s->setUserName(QString());
  s->setPassword(QString());

  DOUT("exit: async =" << async);
}

void
MainWindow::checkInternetConnection(bool async)
{
  DOUT("enter: async =" << async << "connect =" << server_->isConnected());
  if (disposed_) {
    DOUT("exit: returned from disposed branch");
    return;
  }
  if (async) {
    log(tr("connecting to server ..."));
    QThreadPool::globalInstance()->start(new task_::checkInternetConnection(this));
    DOUT("exit: returned from async branch");
    return;
  }

  DOUT("inetMutex locking");
  inetMutex_.lock();
  DOUT("inetMutex locked");

  server_->updateConnected();

  DOUT("inetMutex unlocking");
  inetMutex_.unlock();
  DOUT("inetMutex unlocked");

  if (server_->isConnected())
    emit logged(tr("server connected"));
  else
    emit logged(tr("server disconnected"));

  DOUT("exit");
}

void
MainWindow::deleteCaches()
{
  // TODO
}

void
MainWindow::login(const QString &userName, const QString &encryptedPassword, bool async)
{
  DOUT("enter: async =" << async << ", userName =" << userName);
  if (disposed_) {
    DOUT("exit: returned from disposed branch");
    return;
  }
  if (async) {
    QThreadPool::globalInstance()->start(new task_::login(userName, encryptedPassword, this));
    DOUT("exit: returned from async branch");
    return;
  }

  DOUT("inetMutex locking");
  inetMutex_.lock();
  DOUT("inetMutex locked");

  Settings *settings = Settings::globalInstance();

  server_->updateConnected();
  if (disposed_) {
    DOUT("inetMutex locking"); inetMutex_.lock(); DOUT("inetMutex locked");
    DOUT("exit: returned from disposed branch");
    return;
  }
  if (!server_->isConnected() && cache_->isValid()) {
    // login from cache
    log(TR(T_MESSAGE_TRY_LOGINFROMCACHE));
    User user = cache_->selectUserWithNameAndPassword(userName, encryptedPassword);
    if (user.isValid()) {
      settings->setUserName(userName);
      settings->setPassword(encryptedPassword);

      server_->setAuthorized(true);
      server_->setUser(user);
      annotationBrowser_->setUserId(user.id());
      log(TR(T_SUCCEED_LOGINFROMCACHE));
    } else
      warn(TR(T_ERROR_LOGINFROMCACHE_FAILURE));

    DOUT("inetMutex unlocking");
    inetMutex_.unlock();
    DOUT("inetMutex unlocked");
    DOUT("exit from offline branch");
    return;
  }

  settings->setUserName(userName);
  settings->setPassword(encryptedPassword);

  server_->login(userName, encryptedPassword);
  if (disposed_) {
    DOUT("inetMutex locking"); inetMutex_.lock(); DOUT("inetMutex locked");
    DOUT("exit: returned from disposed branch");
    return;
  }

  bool online = server_->isConnected() && server_->isAuthorized();
  if (online) {
    liveInterval_ = server_->selectLiveTokenInterval();
    if (disposed_) {
      DOUT("inetMutex locking"); inetMutex_.lock(); DOUT("inetMutex locked");
      DOUT("exit: returned from disposed branch");
      return;
    }
    if (liveInterval_ <= 0)
      liveInterval_ = DEFAULT_LIVE_INTERVAL;

    //if (!server_->user().hasLanguage())
    //  server_->setUserLanguage(Traits::UnknownLanguage);

    qint64 uid = server_->user().id();
    settings->setUserId(uid);
    //tokenView_->setUserId(uid);
    annotationView_->setUserId(uid);
    annotationBrowser_->setUserId(uid);

    if (cache_->isValid())
      cache_->updateUser(server_->user());

    // Languages
    //switch (server_->user().language()) {
    //case Traits::Japanese:
    //  toggleAnnotationLanguageToJapaneseAct_->setChecked(true);
    //  break;
    //case Traits::Chinese:
    //  toggleAnnotationLanguageToChineseAct_->setChecked(true);
    //  break;
    //case Traits::Korean:
    //  toggleAnnotationLanguageToKoreanAct_->setChecked(true);
    //  break;
    //case Traits::UnknownLanguage:
    //  toggleAnnotationLanguageToUnknownAct_->setChecked(true);
    //  break;
    //case Traits::English:
    //case Traits::AnyLanguage:
    //default:
    //  toggleAnnotationLanguageToEnglishAct_->setChecked(true);
    //}
    //invalidateAnnotationLanguages();

    QDate today = QDate::currentDate();
    if (Settings::globalInstance()->updateDate() != today) {
      bool updated = server_->isSoftwareUpdated();
      if (!updated)
        warn(tr("new version released, please check Help/Update menu"));

      Settings::globalInstance()->setUpdateDate(today);
    }

    if (disposed_) {
      DOUT("inetMutex locking"); inetMutex_.lock(); DOUT("inetMutex locked");
      DOUT("exit: returned from disposed branch");
      return;
    }
    dataServer_->commitQueue();
  }

  DOUT("inetMutex unlocking");
  inetMutex_.unlock();
  DOUT("inetMutex unlocked");

  emit userChanged(server_->user());

  if (!disposed_ && online &&
      settings->isLive() && !hub_->isSignalTokenMode() && !player_->hasMedia())
    QTimer::singleShot(0, hub_, SLOT(setLiveTokenMode()));

  DOUT("exit");
}

// - Sync mode -

/*
void
MainWindow::setSyncMode(bool t)
{
  bool isSyncMode = hub_->playerMode() == SignalHub::SyncPlayMode;
  if (isSyncMode != t) {
    if (t)
      hub_->setPlayerMode(SignalHub::SyncPlayMode);
    else
      hub_->setPlayerMode(SignalHub::NormalPlayMode);
    emit syncModeChanged(t);

    if (t)
      log(tr("Sync mode started"));
    else
      log(tr("Sync mode stopped"));
  }
}
*/

// - Live mode -

/*
void
MainWindow::setLiveMode(bool t)
{
  bool isLiveMode = hub_->playerMode() == SignalHub::LivePlayMode;
  if (isLiveMode != t) {
    if (t)
      hub_->setPlayerMode(SignalHub::LivePlayMode);
    else
      hub_->setPlayerMode(SignalHub::NormalPlayMode);
    emit liveModeChanged(t);

    if (t)
      log(tr("Live mode started"));
    else
      log(tr("Live mode stopped"));
  }
}
*/

#ifdef USE_WIN_QTH

// - Signal mode -

void
MainWindow::openProcessWindow(WId hwnd)
{
  ulong pid = QtWin::getWindowProcessId(hwnd);
  if (pid) {
    log(tr("found process id for window") + QString(" (pid = %1)").arg(QString::number(pid)));
    openProcessId(pid);
  } else
    warn(tr("process id for window was not found"));
  openProcess();
}

void
MainWindow::openProcessId(ulong pid)
{
  if (!pid)
    return;

  if (QTH->isProcessAttached(pid))
    log(tr("process was attached") + QString(" (pid = %1)").arg(pid));
  else {
    bool ok = QTH->attachProcess(pid);
    if (ok) {
      log(tr("process attached") + QString(" (pid = %1)").arg(QString::number(pid)));
      signalView_->processView()->refresh();
      ProcessInfo pi = signalView_->processView()->attachedProcessInfo();
      if (pi.isValid() && pid == pi.processId)
        emit attached(pi);
    } else {
      error(tr("failed to attach process ") + QString(" (pid = %1)").arg(QString::number(pid)));
      if (!QtWin::isProcessActiveWithId(pid))
        notify(tr("Is the process running now?"));
      else if (!QTH->isElevated())
        notify(tr("Run me as administrator and try again (o^^o)"));
      else
        notify(tr("Restart the target process might help -_-"));
    }
  }
}

void
MainWindow::openProcessPath(const QString &path)
{
  QString processName = QFileInfo(path).fileName();
  ulong pid = QTH->processIdByName(processName);
  if (pid) {
    log(tr("process was started") + QString(" (pid = %1)").arg(pid));
  } else {
    QtWin::createProcessWithExecutablePath(path);
    log(tr("told process to start") + QString(" (name = %1)").arg(processName));
    pid = QTH->processIdByName(processName);
  }

  addRecent(path);

  signalView_->processView()->refresh();
  if (!pid) {
    warn(tr("failed to start process") + QString(" (name = %1)").arg(processName));
    openProcess();
  } else {
    enum { seconds = G_OPENPROCESS_TIMEOUT / 1000 }; // msecs / 1000
    log(tr("wait %1 seconds for process to start ...").arg(QString::number(seconds)));
    auto *slot = new MainWindow_slot_::OpenProcessId(pid, this);
    QTimer::singleShot(G_OPENPROCESS_TIMEOUT, slot, SLOT(openProcessId()));
  }
}

void
MainWindow::openProcessHook(ulong hookId, const ProcessInfo &pi)
{
  if (player_->hasMedia() && !player_->isStopped())
    stop();

  if (pi.isValid()) {
    log(tr("openning process") + ": " + pi.processName);

    grabber_->setBaseName(pi.processName);

    QString title = pi.processName;
    setWindowTitle(title);
    miniPlayer_->setWindowTitle(title); // TODO: move to invalidateTitle!!! or hub_();

    addRecent(pi.executablePath);
    setRecentOpenedFile(pi.executablePath);

    if (hookId != messageHandler_->hookId())
      messageHandler_->clearRecentMessages();

    messageHandler_->setProcessInfo(pi);
    messageHandler_->setHookId(hookId);
  }

  hub_->setSignalTokenMode();

  QString mrl;
  if (pi.isValid())
    mrl = pi.executablePath;
  recentDigest_.clear();
  recentSource_.clear();
  invalidateToken(mrl);

  hub_->play();
}

#endif // USE_WIN_QTH

#ifdef USE_MODE_SIGNAL

void
MainWindow::showSignalView()
{ setSignalViewVisible(true); }

void
MainWindow::hideSignalView()
{ setSignalViewVisible(false); }

void
MainWindow::setSignalViewVisible(bool visible)
{
  //if (visible) {
  //  if (hub_->isMediaTokenMode() && hub_->isNormalPlayerMode() &&
  //      !player_->isPlaying() && !player_->isPaused())
  //    hide();
  //} else {
  //  if (hub_->isMediaTokenMode() && !isVisible())
  //    show();
  //}
  //signalView_->tokenView()->setToken(tokenView_->token());
  //signalView_->tokenView()->setAliases(tokenView_->aliases());
  //signalView_->tokenView()->setUserId(tokenView_->userId());
  signalView_->setVisible(visible);
  if (visible)
    signalView_->raise();
}

void
MainWindow::showRecentMessageView()
{ setRecentMessageViewVisible(true); }

void
MainWindow::hideRecentMessageView()
{ setRecentMessageViewVisible(false); }

void
MainWindow::setRecentMessageViewVisible(bool visible)
{
  if (visible) {
    recentMessageView_->clear();
    recentMessageView_->addMessages(::revertList(messageHandler_->recentMessages()), messageHandler_->hookId());
    recentMessageView_->setCurrentIndex(1);
  }
  recentMessageView_->setVisible(visible);
  if (visible)
    recentMessageView_->raise();
}

#endif // USE_MODE_SIGNAL

// - User -

void
MainWindow::setUserAnonymous(bool t, bool async)
{
  DOUT("enter: async =" << async);
  if (disposed_) {
    DOUT("exit: returned from disposed branch");
    return;
  }

  if (server_->isConnected() && server_->isAuthorized()) {

    if (async) {
      log(tr("connecting server to change anonymous status ..."));
      QThreadPool::globalInstance()->start(new task_::setUserAnonymous(t, this));
      DOUT("exit: returned from async branch");
      return;
    }

    DOUT("inetMutex locking");
    inetMutex_.lock();
    DOUT("inetMutex locked");

    bool ok = server_->setUserAnonymous(t);
    if (!ok)
      warn(tr("failed to change user anonymous state"));

    DOUT("inetMutex unlocking");
    inetMutex_.unlock();
    DOUT("inetMutex unlocked");

  } else
    server_->user().setAnonymous(t);

  if (server_->user().isAnonymous())
    log(tr("you are anonymous now"));
  else
    log(tr("you are not anonymous now"));

  DOUT("exit");
}

void
MainWindow::setUserLanguage(int lang, bool async)
{
  DOUT("enter: async =" << async);
  if (disposed_) {
    DOUT("exit: returned from disposed branch");
    return;
  }

  if (server_->isConnected() && server_->isAuthorized()) {

    if (async) {
      log(tr("connecting server to change language ..."));
      QThreadPool::globalInstance()->start(new task_::setUserLanguage(lang, this));
      DOUT("exit: returned from async branch");
      return;
    }

    DOUT("inetMutex locking");
    inetMutex_.lock();
    DOUT("inetMutex locked");

    bool ok = server_->setUserLanguage(lang);
    if (!ok)
      warn(tr("failed to change user language"));

    DOUT("inetMutex unlocking");
    inetMutex_.unlock();
    DOUT("inetMutex unlocked");

  } else
    server_->user().setLanguage(lang);

  log(tr("your language is ") + languageToString(server_->user().language()));

  DOUT("exit");
}

void MainWindow::setUserLanguageToEnglish()     { setUserLanguage(Traits::English); }
void MainWindow::setUserLanguageToJapanese()    { setUserLanguage(Traits::Japanese); }
void MainWindow::setUserLanguageToChinese()     { setUserLanguage(Traits::Chinese); }
void MainWindow::setUserLanguageToKorean()      { setUserLanguage(Traits::Korean); }
void MainWindow::setUserLanguageToUnknown()     { setUserLanguage(Traits::UnknownLanguage); }

void
MainWindow::invalidateAnnotationLanguages()
{
  qint64 languages = 0;
  if (toggleAnnotationLanguageToAnyAct_->isChecked()) {
    languages = Traits::AnyLanguageBit;

    toggleAnnotationLanguageToEnglishAct_->setEnabled(false);
    toggleAnnotationLanguageToJapaneseAct_->setEnabled(false);
    toggleAnnotationLanguageToChineseAct_->setEnabled(false);
    toggleAnnotationLanguageToKoreanAct_->setEnabled(false);
    toggleAnnotationLanguageToUnknownAct_->setEnabled(false);

  } else {
    toggleAnnotationLanguageToEnglishAct_->setEnabled(true);
    toggleAnnotationLanguageToJapaneseAct_->setEnabled(true);
    toggleAnnotationLanguageToChineseAct_->setEnabled(true);
    toggleAnnotationLanguageToKoreanAct_->setEnabled(true);
    toggleAnnotationLanguageToUnknownAct_->setEnabled(true);

    if (toggleAnnotationLanguageToUnknownAct_->isChecked())       languages |= Traits::UnknownLanguageBit;
    if (toggleAnnotationLanguageToEnglishAct_->isChecked())       languages |= Traits::EnglishBit;
    if (toggleAnnotationLanguageToJapaneseAct_->isChecked())      languages |= Traits::JapaneseBit;
    if (toggleAnnotationLanguageToChineseAct_->isChecked())       languages |= Traits::ChineseBit;
    if (toggleAnnotationLanguageToKoreanAct_->isChecked())        languages |= Traits::KoreanBit;
  }

  // Use at least unknown language
  if (!languages) {
    toggleAnnotationLanguageToUnknownAct_->setChecked(true);
    languages = Traits::UnknownLanguageBit;
  }

  annotationFilter_->setLanguages(languages);
}

// - Language -

void
MainWindow::setAppLanguageToEnglish()
{ Settings::globalInstance()->setLanguage(QLocale::English); invalidateAppLanguage(); }

void
MainWindow::setAppLanguageToJapanese()
{ Settings::globalInstance()->setLanguage(QLocale::Japanese); invalidateAppLanguage(); }

// TODO: Simplified Chinese and Traditional Chinese are not differed until qt 4.8
void
MainWindow::setAppLanguageToTraditionalChinese()
{ Settings::globalInstance()->setLanguage(TranslatorManager::TraditionalChinese); invalidateAppLanguage(); }

void
MainWindow::setAppLanguageToSimplifiedChinese()
{ Settings::globalInstance()->setLanguage(QLocale::Chinese); invalidateAppLanguage(); }

void
MainWindow::invalidateAppLanguage()
{ warn(tr("restart the app to use the new language")); }

// - Helpers -

QString
MainWindow::languageToString(int lang)
{
  switch(lang) {
  case Traits::AnyLanguage:     return TR(T_ANYLANGUAGE);
  case Traits::English:         return TR(T_ENGLISH);
  case Traits::Japanese:        return TR(T_JAPANESE);
  case Traits::Chinese:         return TR(T_CHINESE);
  case Traits::Korean:          return TR(T_KOREAN);

  case Traits::UnknownLanguage:
  default : return TR(T_ALIEN);
  }
}


// - Playlist -

void
MainWindow::openPlaylistItem(int i)
{
  if (i >= 0 && i < playlist_.size())
    openSource(playlist_[i]);
}

void
MainWindow::clearPlaylist()
{
  playlist_.clear();
  invalidatePlaylistMenu();
}

void
MainWindow::invalidatePlaylist()
{
  if (playlist_.isEmpty())
    return;
  bool update = false;

  BOOST_AUTO(p, playlist_.begin());
  while (p != playlist_.end()) {
    QFileInfo fi(removeUrlPrefix(*p));
    if (fi.exists())
      ++p;
    else {
      p = playlist_.erase(p);
      update = true;
    }
  }
  if (update)
    invalidatePlaylistMenu();
}

void
MainWindow::openNextPlaylistItem()
{
  if (playlist_.size() <= 1)
    return;

  int i = playlist_.indexOf(recentOpenedFile_);
  i++;
  if (i < 0 || i >= playlist_.size())
    i = 0;
  openSource(playlist_[i]);
}

int
MainWindow::currentPlaylistIndex() const
{ return playlist_.indexOf(recentOpenedFile_); }

void
MainWindow::openPreviousPlaylistItem()
{
  if (playlist_.size() <= 1)
    return;
  if (playlist_.size() <= 1)
    return;

  int i = playlist_.indexOf(recentOpenedFile_);
  i--;
  if (i < 0 || i >= playlist_.size())
    i = 0;
  openSource(playlist_[i]);
}

void
MainWindow::invalidatePlaylistMenu()
{
  QList<QAction*> l = playlistMenu_->actions();
  foreach (QAction *a, l)
    if (a && a != clearPlaylistAct_ && a != previousPlaylistItemAct_ && a != nextPlaylistItemAct_)
      a->deleteLater();
  playlistMenu_->clear();

  if (!playlist_.isEmpty()) {
    int i = playlist_.indexOf(recentOpenedFile_);
    playlistMenu_->addAction(clearPlaylistAct_);
    if (i > 0)
      playlistMenu_->addAction(previousPlaylistItemAct_);
    if (i < playlist_.size() - 1)
      playlistMenu_->addAction(nextPlaylistItemAct_);
    playlistMenu_->addSeparator();

    int index = 0;
    foreach (QString f, playlist_) {
      QString path = removeUrlPrefix(f);
      QString text = QFileInfo(path).fileName();
      if (text.isEmpty()) {
        text = QDir(path).dirName();
        if (text.isEmpty())
          text = f;
      }
      text = QString::number(index+1) + ". " + shortenText(text);
      BOOST_AUTO(a, new QtExt::ActionWithId(index++, text, playlistMenu_));
      if (f == recentOpenedFile_) {
#ifdef Q_WS_X11
        a->setCheckable(true);
        a->setChecked(true);
#else
        a->setIcon(QIcon(RC_IMAGE_CURRENTFILE));
#endif // Q_WS_X11
      }
      connect(a, SIGNAL(triggeredWithId(int)), SLOT(openPlaylistItem(int)));

      playlistMenu_->addAction(a);
    }
  }
}

// - Recent -

void
MainWindow::invalidateRecent()
{
  if (recentFiles_.isEmpty())
    return;
  bool update = false;

  BOOST_AUTO(p, recentFiles_.begin());
  while (p != recentFiles_.end()) {
    if (isRemoteMrl(*p) ||
        QFileInfo(removeUrlPrefix(*p)).exists())
      ++p;
    else {
      p = recentFiles_.erase(p);
      update = true;
    }
  }

  if (!recentFiles_.isEmpty()) {
     QStringList unique = ::uniqueList(recentFiles_);
     if (unique.size() != recentFiles_.size()) {
       recentFiles_ = unique;
       update = true;
     }
  }

  if (update)
    invalidateRecentMenu();
}

void
MainWindow::invalidateRecentMenu()
{
  QList<QAction*> l = recentMenu_->actions();
  foreach (QAction *a, l)
    if (a && a != clearRecentAct_)
      a->deleteLater();
  recentMenu_->clear();

  if (!recentFiles_.isEmpty()) {
    int i = 0;
    foreach (QString f, recentFiles_) {
      if (i >= G_RECENT_COUNT)
        break;

      QString text;
      if (isRemoteMrl(f))
        text = f;
      else {
        QString path = removeUrlPrefix(f);
        text = QFileInfo(path).fileName();
        if (text.isEmpty()) {
          text = QDir(path).dirName();
          if (text.isEmpty())
            text = f;
        }
      }
      Q_ASSERT(!text.isEmpty());
      text = QString::number(i+1) + ". " + shortenText(text);
      BOOST_AUTO(a, new QtExt::ActionWithId(i++, text, recentMenu_));
      connect(a, SIGNAL(triggeredWithId(int)), SLOT(openRecent(int)));

      recentMenu_->addAction(a);
    }

    recentMenu_->addSeparator();
    recentMenu_->addAction(clearRecentAct_);
  }
}

void
MainWindow::clearRecent()
{
  recentFiles_.clear();
  invalidateRecentMenu();
}

void
MainWindow::openRecent(int i)
{
  if (i >= 0 && i < recentFiles_.size())
    openSource(recentFiles_[i]);
}

void
MainWindow::openSource(const QString &path)
{
  if (path.isEmpty())
    return;

  if (isRemoteMrl(path)) {
    openUrl(path);
    return;
  }

  QFileInfo fi(removeUrlPrefix(path));
  if (!fi.exists()) {
    warn(TR(T_ERROR_BAD_FILEPATH) + ": " + path);
    return;
  }

#ifdef Q_WS_WIN
  if (fi.suffix().compare("lnk", Qt::CaseInsensitive) == 0) {
    QString lnk = QtWin::resolveLink(path, winId());
    if (!lnk.isEmpty()) {
      addRecent(path);
      openSource(lnk);
    } else
      warn(tr("invalid lnk") + ": " + path );
  } else
#endif // Q_WS_WIN
#ifdef USE_MODE_SIGNAL
  if (fi.suffix().compare("exe", Qt::CaseInsensitive) == 0)
    openProcessPath(path);
  else
#endif // USE_MODE_SIGNAL
  openMrl(path, true);
}

void
MainWindow::addRecent(const QString &path)
{
  //if (recentFiles_.indexOf(path) >= 0)
  if (!recentFiles_.isEmpty())
    recentFiles_.removeAll(path);
  recentFiles_.prepend(path);

  // SingleShot to avoid multi-threading issue.
  QTimer::singleShot(0, this, SLOT(invalidateRecentMenu()));
}

// - Translate -

bool
MainWindow::isTranslateEnabled() const
{ return hub_->isSignalTokenMode() && toggleTranslateAct_->isChecked(); }

void
MainWindow::setTranslateEnabled(bool enabled)
{ toggleTranslateAct_->setChecked(enabled); }

void
MainWindow::translate(const QString &text)
{
  if (!isTranslateEnabled() || text.isEmpty())
    return;

  if (!server_->isConnected() || !server_->isAuthorized())
    return;

  int l = server_->user().language();
  QString lcode;
  switch (l) {
  case Traits::English:  lcode = Translator::English; break;
  case Traits::Japanese: lcode = Translator::Japanese; break;
  case Traits::Chinese:  lcode = Translator::SimplifiedChinese; break;
  case Traits::Korean:   lcode = Translator::Korean; break;
  default: return;
  }

  translator_->translate(text, lcode);
}

// - Subtitle -

void
MainWindow::setSubtitleOnTop(bool t)
{
  annotationView_->setSubtitlePosition(
        t ? AnnotationGraphicsView::AP_Top
          : AnnotationGraphicsView::AP_Bottom);

  Settings::globalInstance()->setSubtitleOnTop(t);
}

bool
MainWindow::isSubtitleOnTop() const
{ return annotationView_->subtitlePosition() == AnnotationGraphicsView::AP_Top; }

void
MainWindow::uncheckSubtitleColorActions()
{
  setSubtitleColorToDefaultAct_->setChecked(false);
  setSubtitleColorToWhiteAct_->setChecked(false);
  setSubtitleColorToCyanAct_->setChecked(false);
  setSubtitleColorToBlueAct_->setChecked(false);
  setSubtitleColorToPurpleAct_->setChecked(false);
  setSubtitleColorToBlackAct_->setChecked(false);
  setSubtitleColorToOrangeAct_->setChecked(false);
  setSubtitleColorToRedAct_->setChecked(false);
}


void
MainWindow::setSubtitleColorToDefault()
{
  annotationView_->setSubtitlePrefix(QString());
  uncheckSubtitleColorActions();
  setSubtitleColorToDefaultAct_->setChecked(true);
}

#define SET_SUBTITLE_COLOR(_Color, _COLOR) \
  void \
  MainWindow::setSubtitleColorTo##_Color() \
  { \
    QString prefix = CORE_CMD_COLOR_##_COLOR " "; \
    annotationView_->setSubtitlePrefix(prefix); \
    uncheckSubtitleColorActions(); \
    setSubtitleColorTo##_Color##Act_->setChecked(true); \
  }

  SET_SUBTITLE_COLOR(White, WHITE)
  SET_SUBTITLE_COLOR(Cyan, CYAN)
  SET_SUBTITLE_COLOR(Black, BLACK)
  SET_SUBTITLE_COLOR(Blue, BLUE)
  SET_SUBTITLE_COLOR(Orange, ORANGE)
  SET_SUBTITLE_COLOR(Purple, PURPLE)
  SET_SUBTITLE_COLOR(Red, RED)
#undef SET_SUBTITLE_COLOR

void
MainWindow::setSubtitleColor(int c)
{
  switch (c) {
  case Black:   setSubtitleColorToBlack(); break;
  case White:   setSubtitleColorToWhite(); break;
  case Cyan:    setSubtitleColorToCyan(); break;
  case Blue:    setSubtitleColorToBlue(); break;
  case Red:     setSubtitleColorToRed(); break;
  case Orange:  setSubtitleColorToOrange(); break;
  case Purple:  setSubtitleColorToPurple(); break;
  case DefaultColor:
  default:      setSubtitleColorToDefault();
  }
}

int
MainWindow::subtitleColor() const
{
  if (setSubtitleColorToWhiteAct_->isChecked())
    return White;
  if (setSubtitleColorToCyanAct_->isChecked())
    return Cyan;
  if (setSubtitleColorToBlackAct_->isChecked())
    return Black;
  if (setSubtitleColorToBlueAct_->isChecked())
    return Blue;
  if (setSubtitleColorToRedAct_->isChecked())
    return Red;
  if (setSubtitleColorToOrangeAct_->isChecked())
    return Orange;
  if (setSubtitleColorToPurpleAct_->isChecked())
    return Purple;
  if (setSubtitleColorToDefaultAct_->isChecked())
    return DefaultColor;
  return DefaultColor;
}


// - Themes -

#define SET_THEME_TO(_theme) \
  void \
  MainWindow::setThemeTo##_theme() \
  { \
    UiStyle::globalInstance()->setTheme(UiStyle::_theme##Theme); \
\
    if (hub_->isMediaTokenMode() && hub_->isEmbeddedPlayerMode() && \
        player_->hasMedia() && \
        !UiStyle::isAeroAvailable()) \
      UiStyle::globalInstance()->setBlackBackground(this); \
  }

  SET_THEME_TO(Default)
  SET_THEME_TO(Random)
  SET_THEME_TO(Dark)
  SET_THEME_TO(Black)
  SET_THEME_TO(Blue)
  SET_THEME_TO(Brown)
  SET_THEME_TO(Cyan)
  SET_THEME_TO(Gray)
  SET_THEME_TO(Green)
  SET_THEME_TO(Pink)
  SET_THEME_TO(Purple)
  SET_THEME_TO(Red)
  SET_THEME_TO(White)
  SET_THEME_TO(Yellow)
#undef SET_THEME_TO

void
MainWindow::enableWindowTransparency()
{ setWindowOpacity(G_WINDOW_OPACITY); }

void
MainWindow::disableWindowTransparency()
{ setWindowOpacity(1.0); }

// - Browse -

void
MainWindow::setBrowsedFile(const QString &filePath)
{
  browsedFiles_.clear();
  browseMenu_->clear();

  QFileInfo fi(filePath);
  QDir dir = fi.dir();

  if (!dir.exists())
    return;

  int tt = fileType(filePath);
  QStringList nf;
  switch (tt) {
  case Token::TT_Video:         nf = Player::supportedVideoFilters(); break;
  case Token::TT_Audio:         nf = Player::supportedAudioFilters(); break;
  case Token::TT_Picture:       nf = Player::supportedPictureFilters(); break;
  }
  if (nf.isEmpty())
    return;

  browsedFiles_ = dir.entryInfoList(nf, QDir::Files);
  if (browsedFiles_.size() <= 1)
    return;

  if (browsedFiles_.size() > 10) {
    browseMenu_->addAction(previousFileAct_);
    browseMenu_->addAction(nextFileAct_);
    browseMenu_->addSeparator();
  }

  int id = 0;
  foreach (QFileInfo f, browsedFiles_) {
    QString text = QString::number(id+1) + ". " + f.fileName();
    BOOST_AUTO(a, new QtExt::ActionWithId(id++, text, browseMenu_));
    if (f.fileName() == fi.fileName()) {
#ifdef Q_WS_X11
      a->setCheckable(true);
      a->setChecked(true);
#else
      a->setIcon(QIcon(RC_IMAGE_CURRENTFILE));
#endif // Q_WS_X11
    }
    connect(a, SIGNAL(triggeredWithId(int)), SLOT(openBrowsedFile(int)));
    browseMenu_->addAction(a);
  }
}

void
MainWindow::openBrowsedFile(int id)
{
  if (id < 0)
    id = browsedFiles_.size() - 1;
  else if (id >= browsedFiles_.size())
    id = 0;

  if (id >= 0 && id < browsedFiles_.size())
    openMrl(browsedFiles_[id].absoluteFilePath());
}

int
MainWindow::currentBrowsedFileId() const
{
  if (browsedFiles_.isEmpty())
    return 0;

  QString path;
  if (player_->hasMedia())
    path = player_->mediaPath();
  if (path.isEmpty())
    return 0;

  QFileInfo fi(path);
  //if (!fi.exists())
  //  return 0;

  int ret = 0;
  foreach (QFileInfo f, browsedFiles_) {
    if (f.fileName() == fi.fileName())
      break;
    ret++;
  }
  return ret;
}

void
MainWindow::openPreviousFile()
{ openBrowsedFile(currentBrowsedFileId() - 1); }

void
MainWindow::openNextFile()
{ openBrowsedFile(currentBrowsedFileId() + 1); }

// - Previous / Next -

void
MainWindow::previous()
{
  if (hub_->isMediaTokenMode() && player_->hasMedia()) {
    if (player_->hasPlaylist())
      player_->previousTrack();
    //else if (player_->hasChapters())
    //  player_->setPreviousChapter();
    else if (player_->titleCount() > 1)
      player_->setPreviousTitle();
    else if (playlist_.size() > 1)
      openPreviousPlaylistItem();
    else if (browsedFiles_.size() > 1)
      openPreviousFile();
  }
}

void
MainWindow::next()
{
  if (hub_->isMediaTokenMode() && player_->hasMedia()) {
    if (player_->hasPlaylist())
      player_->nextTrack();
    //else if (player_->hasChapters())
    //  player_->setNextChapter();
    else if (player_->titleCount() > 1)
      player_->setNextTitle();
    else if (playlist_.size() > 1)
      openNextPlaylistItem();
    else if (browsedFiles_.size() > 1)
      openNextFile();
  }
}

void
MainWindow::autoPlayNext()
{
  if (isAutoPlayNext())
    next();
}

// - Tracking window -

void
MainWindow::setEmbeddedWindow(WId winId)
{
  if (!player_->isValid())
    return;

  if (!winId)
    player_->embed(videoView_);
  else {
#ifndef Q_WS_MAC
    player_->embed(0);
    player_->setEmbeddedWindow(winId);
#endif // !Q_WS_MAC
  }
}

// - Live mode -

void
MainWindow::openChannel()
{
  Q_ASSERT(liveInterval_ > 0);
  annotationView_->setInterval(liveInterval_);
  liveTimer_->setInterval(liveInterval_);

  annotationView_->start();
  liveTimer_->start();
}

void
MainWindow::closeChannel()
{
  liveTimer_->stop();
  annotationView_->stop();
}

void
MainWindow::updateLiveAnnotations(bool async)
{
  if (disposed_) {
    DOUT("exit: returned from disposed branch");
    return;
  }
  if (!server_->isConnected() || !server_->isAuthorized())
    return;
  //DOUT("enter: async =" << async);

  if (async) {
    //log(tr("connecting server to update annot ..."));
    QThreadPool::globalInstance()->start(new task_::updateLiveAnnotations(this));
    //DOUT("exit: returned from async branch");
    return;
  }

  //DOUT("inetMutex locking");
  inetMutex_.lock();
  //DOUT("inetMutex locked");

  AnnotationList l = server_->selectLiveAnnotations();
  //DOUT("annots.count =" << l.size());

  if (!l.isEmpty())
    //annotationView_->appendAnnotations(l);
    emit appendAnnotationsRequested(l);

  //DOUT("inetMutex unlocking");
  inetMutex_.unlock();
  //DOUT("inetMutex unlocked");

  //DOUT("exit");
}

// - Remote annotation -

void
MainWindow::addRemoteAnnotations(const AnnotationList &l, const QString &url)
{
  AnnotationList annots;
  const Token &t = dataManager_->token();
  foreach (Annotation a, l) {
    a.setTokenId(t.id());
    a.setTokenPart(t.part());
    a.setTokenDigest(t.digest());
    annots.append(a);
  }
  if (!annots.isEmpty()) {
    QString msg = QString("%1 :" HTML_STYLE_OPEN(color:red) "+%2" HTML_STYLE_CLOSE() ", %3")
      .arg(tr("annotations found"))
      .arg(QString::number(annots.size()))
      .arg(url);
    emit logged(msg);
    emit addAnnotationsRequested(annots);
    if (t.hasId())
      dataServer_->updateAnnotations(annots);
  }
}

void
MainWindow::importAnnotationsFromUrl(const QString &suburl)
{
  if (!suburl.isEmpty()) {
    log(tr("analyzing annotation URL ...") + ": " + suburl);
    if (registerAnnotationUrl(suburl))
      AnnotationCodecManager::globalInstance()->fetch(suburl);
  }
}

bool
MainWindow::registerAnnotationUrl(const QString &suburl)
{
  if (suburl.isEmpty() ||
      annotationUrls_.contains(suburl, Qt::CaseInsensitive))
    return false;
  annotationUrls_.append(suburl);
  return true;
}

void
MainWindow::clearAnnotationUrls()
{ annotationUrls_.clear(); }

// - Source -

QString
MainWindow::currentUrl() const
{
  QString ret;
  if (dataManager_->token().hasSource())
    ret = dataManager_->token().source();
  else if (dataManager_->hasAliases())
    foreach (const Alias &a, dataManager_->aliases())
      if (a.type() == Alias::AT_Url) {
        ret = a.text();
        if (ret.contains("bilibili.tv/", Qt::CaseInsensitive) ||
            ret.contains("nicovideo.jp/", Qt::CaseInsensitive))
          break;
      }

  return isRemoteMrl(ret) ? ret : QString();
}

void
MainWindow::downloadCurrentUrl()
{
  DownloadDialog *dlg = downloadDialog();
  QString url = currentUrl();
  if (!url.isEmpty())
    dlg->promptDownloads(url);
  dlg->show();
}

void
MainWindow::openInWebBrowser()
{
  QString url = currentUrl();
  if (!url.isEmpty())
    openInCloudView(url);
}

// - Error handling -

void
MainWindow::handlePlayerError()
{ }

// - Helpers -

QString
MainWindow::shortenText(const QString &text)
{
  if (text.size() <= 30)
    return text;
  else
    return text.left(17) + "..." + text.right(10);
}

bool
MainWindow::isRemoteMrl(const QString& mrl)
{ return mrl.contains(QRegExp("[ts]p://", Qt::CaseInsensitive)); }

// - Resume -

void
MainWindow::rememberPlayPos()
{
  DOUT("enter");
  enum { margin = 90 * 1000 }; // 1.5min
  if (player_->hasMedia()) {
    DOUT("hasMedia = true");

    qint64 hash = dataManager_->token().hashId();
    DOUT("hash =" << hash);
    if (!hash) {
      DOUT("exit: no hash");
      return;
    }

    qint64 pos = player_->time();
    DOUT("pos =" << pos);
    if (pos <= margin) { // 1.5min
      if (playPosHistory_.contains(hash))
        playPosHistory_.remove(hash);
      DOUT("exit: pos too small");
      return;
    }
    qint64 len = player_->mediaLength();
    DOUT("length =" << len);
    if (pos >= len - margin) {
      if (playPosHistory_.contains(hash))
        playPosHistory_.remove(hash);
      DOUT("exit: pos too large");
      return;
    }

    if (playPosHistory_.size() > 100) {
      // maximum play pos count to remember
      int i = qrand() % playPosHistory_.size();
      qint64 k = playPosHistory_.keys()[i];
      playPosHistory_.remove(k);
    }
    playPosHistory_[hash] = pos;
  }
  DOUT("exit");
}

void
MainWindow::resumePlayPos()
{
  DOUT("enter");
  if (player_->hasMedia() && !player_->isStopped()) {
    DOUT("hasMedia = true");
    resumePlayTimer_->stop();
    if (player_->time() > 20 * 1000) { // 20 seconds, user manually seeked
      DOUT("exit: user manually seeked");
      return;
    }
    qint64 hash = dataManager_->token().hashId();
    DOUT("hash =" << hash);
    if (!hash) {
      DOUT("exit: no hash");
      return;
    }

    qint64 pos = 0;
    if (playPosHistory_.contains(hash))
      pos = playPosHistory_[hash];
    DOUT("pos =" << pos);
    if (pos > 0 && pos < player_->mediaLength()) {
      log(tr("resuming last play") + ": " + QtExt::msecs2time(pos).toString("hh:mm:ss"));
      pos -= 5000; // seek back 5 seconds
      seek(pos);
    }
  }
  DOUT("exit");
}

void
MainWindow::rememberSubtitle()
{
  DOUT("enter");
  if (player_->hasMedia() && player_->hasSubtitles()) {
    DOUT("hasMedia = true");
    int id = player_->isSubtitleVisible() ? player_->subtitleId() : -1;
    DOUT("id =" << id);

    qint64 hash = dataManager_->token().hashId();
    DOUT("hash =" << hash);
    if (!hash) {
      DOUT("exit: no hash");
      return;
    }

    if (id)
      subtitleHistory_[hash] = id;
    else
      subtitleHistory_.remove(hash);

    if (subtitleHistory_.size() > 100) {
      // maximum play pos count to remember
      int i = qrand() % subtitleHistory_.size();
      qint64 k = subtitleHistory_.keys()[i];
      subtitleHistory_.remove(k);
    }
  }
  DOUT("exit");
}

void
MainWindow::resumeSubtitle()
{
  DOUT("enter");
  if (player_->hasMedia() && player_->hasSubtitles()) {
    DOUT("hasMedia = true");
    resumeSubtitleTimer_->stop();
    qint64 hash = dataManager_->token().hashId();
    DOUT("hash =" << hash);
    if (!hash) {
      DOUT("exit: no hash");
      return;
    }

    int id = 0;
    if (subtitleHistory_.contains(hash))
      id = subtitleHistory_[hash];
    DOUT("id =" << id);
    if (id >= 0 && id < player_->subtitleCount() &&
        id != player_->subtitleId()) {
      if (id > 0) {
        log(tr("loading last subtitle") + ": " +
          player_->subtitleDescriptions()[id]);
        player_->setSubtitleId(id);
      }
      player_->setSubtitleVisible(true);
    } else if (id == -1) {
      log(tr("hide last subtitle"));
      player_->setSubtitleVisible(false);
    }
  }
  DOUT("exit");
}

void
MainWindow::rememberAudioTrack()
{
  DOUT("enter");
  if (player_->hasMedia() && player_->audioTrackCount() > 1) {
    DOUT("hasMedia = true");
    int id = player_->audioTrackId();
    DOUT("id =" << id);
    if (id < 0) {
      DOUT("exit: invalid id");
      return;
    }

    qint64 hash = dataManager_->token().hashId();
    DOUT("hash =" << hash);
    if (!hash) {
      DOUT("exit: no hash");
      return;
    }

    if (id)
      audioTrackHistory_[hash] = id;
    else
      audioTrackHistory_.remove(hash);

    if (audioTrackHistory_.size() > 100) {
      // maximum play pos count to remember
      int i = qrand() % audioTrackHistory_.size();
      qint64 k = audioTrackHistory_.keys()[i];
      audioTrackHistory_.remove(k);
    }
  }
  DOUT("exit");
}

void
MainWindow::resumeAudioTrack()
{
  DOUT("enter");
  if (player_->hasMedia() && player_->audioTrackCount() > 1) {
    DOUT("hasMedia = true");
    resumeAudioTrackTimer_->stop();
    qint64 hash = dataManager_->token().hashId();
    DOUT("hash =" << hash);
    if (!hash) {
      DOUT("exit: no hash");
      return;
    }

    int id = 0;
    if (audioTrackHistory_.contains(hash))
      id = audioTrackHistory_[hash];
    DOUT("id =" << id);
    if (id >= 0 && id < player_->audioTrackCount()
        && id != player_->audioTrackId()) {
      if (id > 0)
        log(tr("loading last audio track") + ": " +
            player_->audioTrackDescriptions()[id]);
      player_->setAudioTrackId(id);
    }
  }
  DOUT("exit");
}

// - Effect -

void
MainWindow::invalidateAnnotationEffectMenu()
{
  AnnotationGraphicsView::RenderHint hint = annotationView_->renderHint();
  setAnnotationEffectToDefaultAct_->setChecked(hint == AnnotationGraphicsView::DefaultRenderHint);
  setAnnotationEffectToTransparentAct_->setChecked(hint == AnnotationGraphicsView::TransparentHint);
  setAnnotationEffectToShadowAct_->setChecked(hint == AnnotationGraphicsView::ShadowHint);
  setAnnotationEffectToBlurAct_->setChecked(hint == AnnotationGraphicsView::BlurHint);
}

void
MainWindow::setAnnotationEffectToDefault()
{ annotationView_->setRenderHint(AnnotationGraphicsView::DefaultRenderHint); }
void
MainWindow::setAnnotationEffectToTransparent()
{ annotationView_->setRenderHint(AnnotationGraphicsView::TransparentHint); }
void
MainWindow::setAnnotationEffectToShadow()
{ annotationView_->setRenderHint(AnnotationGraphicsView::ShadowHint); }
void
MainWindow::setAnnotationEffectToBlur()
{ annotationView_->setRenderHint(AnnotationGraphicsView::BlurHint); }

// EOF

/*
//! [updateFormatsTable() part1]
void DropSiteWindow::updateFormatsTable(const QMimeData *mimeData)
{
    formatsTable->setRowCount(0);
    if (!mimeData)
        return;
//! [updateFormatsTable() part1]

//! [updateFormatsTable() part2]
    foreach (const QString &format, mimeData->formats()) {
        QTableWidgetItem *formatItem = new QTableWidgetItem(format);
        formatItem->setFlags(Qt::ItemIsEnabled);
        formatItem->setTextAlignment(Qt::AlignTop | Qt::AlignLeft);
//! [updateFormatsTable() part2]

//! [updateFormatsTable() part3]
        QString text;
        if (format == "text/plain") {
            text = mimeData->text().simplified();
        } else if (format == "text/html") {
            text = mimeData->html().simplified();
        } else if (format == "text/uri-list") {
            QList<QUrl> urlList = mimeData->urls();
            for (int i = 0; i < urlList.size() && i < 32; ++i)
                text.append(urlList[i].toString() + " ");
        } else {
            QByteArray data = mimeData->data(format);
            for (int i = 0; i < data.size() && i < 32; ++i) {
                QString hex = QString("%1").arg(uchar(data[i]), 2, 16,
                                                QChar('0'))
                                           .toUpper();
                text.append(hex + " ");
            }
        }
//! [updateFormatsTable() part3]

//! [updateFormatsTable() part4]
        int row = formatsTable->rowCount();
        formatsTable->insertRow(row);
        formatsTable->setItem(row, 0, new QTableWidgetItem(format));
        formatsTable->setItem(row, 1, new QTableWidgetItem(text));
    }

    formatsTable->resizeColumnToContents(0);
}
//! [updateFormatsTable() part4]
*/
