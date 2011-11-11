// osdplayer.cc
// 7/16/2011

#include "osdplayer.h"
#include "global.h"
#include "tr.h"
#include "stylesheet.h"
#include "uistyle.h"
#ifdef USE_WIN_QTWIN
  #include "win/qtwin/qtwin.h"
#endif // USE_WIN_QTWIN
#include "core/gui/toolbutton.h"
#include <QtGui>

#define DEBUG "OSDPlayerUi"
#include "module/debug/debug.h"

// - Constructions -
OSDPlayerUi::OSDPlayerUi(SignalHub *hub, Player *player, ServerAgent *server, QWidget *parent)
  : Base(hub, player, server, parent), menuButton_(0), trackingWindow_(0)
{
  setWindowFlags(Qt::FramelessWindowHint);
  setContentsMargins(0, 0, 0, 0);
  createLayout();

  // Auto hide player.
  autoHideTimer_ = new QTimer(this);
  autoHideTimer_->setInterval(G_AUTOHIDE_TIMEOUT);
  connect(autoHideTimer_, SIGNAL(timeout()), SLOT(autoHide()));

  trackingTimer_ = new QTimer(this);
  trackingTimer_->setInterval(G_TRACKING_INTERVAL);
  connect(trackingTimer_, SIGNAL(timeout()), SLOT(invalidateGeometry()));

  connect(prefixLineEdit(), SIGNAL(textChanged(QString)), SLOT(resetAutoHideTimeoutWhenEditing(QString)));
  connect(lineEdit(), SIGNAL(textChanged(QString)), SLOT(resetAutoHideTimeoutWhenEditing(QString)));
  connect(prefixLineEdit(), SIGNAL(cursorPositionChanged(int,int)), SLOT(resetAutoHideTimeoutWhenEditing(int,int)));
  connect(lineEdit(), SIGNAL(cursorPositionChanged(int,int)), SLOT(resetAutoHideTimeoutWhenEditing(int,int)));
}

void
OSDPlayerUi::createLayout()
{
  // Reset Ui style
  prefixLineEdit()->setStyleSheet(SS_PREFIXLINEEDIT_OSD);
  lineEdit()->setStyleSheet(SS_LINEEDIT_OSD);

  // Set layout
  QVBoxLayout *rows = new QVBoxLayout; {
    QHBoxLayout *row = new QHBoxLayout;
    rows->addWidget(positionSlider());
    rows->addLayout(row);

    row->addWidget(menuButton());
    row->addWidget(playButton());
    row->addWidget(toggleAnnotationButton());
    row->addWidget(nextFrameButton());
    row->addWidget(stopButton());
    row->addWidget(openButton());
    row->addWidget(toggleMiniModeButton());
    row->addWidget(togglePlayModeButton());
    row->addStretch();
    row->addWidget(userButton());
    row->addWidget(prefixLineEdit());
    row->addWidget(lineEdit());
    row->addWidget(volumeSlider());
    row->addWidget(positionButton());
  }
  setLayout(rows);
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

// - Auto hide -

void
OSDPlayerUi::autoHide()
{
  if (underMouse() || lineEdit()->hasFocus() || prefixLineEdit()->hasFocus())
    resetAutoHideTimeout();
  else if (isVisible())
    hide();
}

bool
OSDPlayerUi::autoHideEnabled() const
{
  Q_ASSERT(autoHideTimer_);
  return autoHideTimer_->isActive();
}

void
OSDPlayerUi::setAutoHideEnabled(bool enabled)
{
  Q_ASSERT(autoHideTimer_);
  if (enabled) {
    if (!autoHideTimer_->isActive())
      autoHideTimer_->start();
  } else  {
    if (autoHideTimer_->isActive())
      autoHideTimer_->stop();
  }
}

void
OSDPlayerUi::resetAutoHideTimeout()
{
  Q_ASSERT(autoHideTimer_);
  if (autoHideTimer_->isActive())
    autoHideTimer_->stop();
  autoHideTimer_->start();
}

// - Geometry -

void
OSDPlayerUi::invalidateGeometry()
{
  if (trackingWindow_) {
#ifdef USE_WIN_QTWIN
    QRect r = QtWin::getWindowRect(trackingWindow_);
    if (r.isNull()) {
      if (!QtWin::isValidWindow(trackingWindow_))
        setTrackingWindow(0);
    } else {

      int w_max = r.width();
      int h_hint = sizeHint().height();
      QSize newSize(w_max, h_hint);
      if (newSize != size())
        resize(newSize);

      int x_left = r.x();
      int y_bottom = r.bottom() - height();

      moveToGlobalPos(QPoint(x_left, y_bottom));
    }
#endif // USE_WIN_QTWIN
  } else if(parentWidget()) {

    // Invalidate size
    int w_max = parentWidget()->width();
    int h_hint = sizeHint().height();
    resize(w_max, h_hint);

    // Invalidate position
    //int x_center = widget->width() / 2 - 2 * width();
    int x_left = 0;
    int y_bottom = parentWidget()->height() - height();
    move(x_left, y_bottom);
  }
}

void
OSDPlayerUi::moveToGlobalPos(const QPoint &globalPos)
{
  // Currently only work on Windows
  QPoint newPos = frameGeometry().topLeft() + pos() // relative position
                  + globalPos - mapToGlobal(pos()); // absolute distance
  if (newPos != pos())
    move(newPos);
}

WId
OSDPlayerUi::trackingWindow() const
{ return trackingWindow_; }

void
OSDPlayerUi::setTrackingWindow(WId hwnd)
{
  if (trackingWindow_ != hwnd) {
     trackingWindow_ = hwnd;
     if (trackingWindow_)
       startTracking();
     else
       stopTracking();
  }
}

void
OSDPlayerUi::startTracking()
{
  if (!trackingTimer_->isActive())
    trackingTimer_->start();
}

void
OSDPlayerUi::stopTracking()
{
  if (trackingTimer_->isActive())
    trackingTimer_->stop();
}

void
OSDPlayerUi::setVisible(bool visible)
{
  if (visible == isVisible())
    return;

  if (visible) {
    if (trackingWindow_) {
      invalidateGeometry();
      startTracking();
    }
  } else
    stopTracking();

  Base::setVisible(visible);
}

// - Menu button -

void
OSDPlayerUi::setMenu(QMenu *menu)
{ menuButton()->setMenu(menu); }

QToolButton*
OSDPlayerUi::menuButton()
{
  if (!menuButton_) {
    menuButton_ = new Core::Gui::ToolButton(this);
    menuButton_->setStyleSheet(SS_TOOLBUTTON_MENU);
    menuButton_->setToolTip(TR(T_TOOLTIP_MENU));
    UiStyle::globalInstance()->setToolButtonStyle(menuButton_);

    connect(menuButton_, SIGNAL(clicked()), SLOT(popupMenu()));
  }
  return menuButton_;
}

void
OSDPlayerUi::popupMenu()
{
  emit invalidateMenuRequested();
  menuButton()->showMenu();
}

// EOF
