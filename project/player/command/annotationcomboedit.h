#ifndef ANNOTATIONCOMBOEDIT_H
#define ANNOTATIONCOMBOEDIT_H

// annotationcomboedit.h
// 7/16/2011

#include "comboedit.h"
#include "module/qtext/withsizehint.h"
#include <QStringList>

QT_FORWARD_DECLARE_CLASS(QAction)
QT_FORWARD_DECLARE_CLASS(QMenu)

class AnnotationEditor;

class AnnotationComboEdit : public ComboEdit, public QtExt::WithSizeHint
{
  Q_OBJECT
  typedef AnnotationComboEdit Self;
  typedef ComboEdit Base;

public:
  explicit AnnotationComboEdit(QWidget *parent = 0);

  // - Properties -
public slots:
  virtual QSize sizeHint() const ///< \override QWidget
  { return QtExt::WithSizeHint::sizeHint(); }

protected slots:
  void edit();

  // - Events -
protected:
  //virtual void keyPressEvent(QKeyEvent *event); ///< \override
  virtual void contextMenuEvent(QContextMenuEvent *event); ///< \override

private:
  void createActions();

private:
  AnnotationEditor *editor_;

protected:
  QAction *editAct;
};

#endif // ANNOTATIONCOMBOEDIT_H
