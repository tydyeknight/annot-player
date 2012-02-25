# qtext.pri
# 6/28/2011

include(../../config.pri)

HEADERS += \
    $$PWD/actionwithid.h \
    $$PWD/bitwise.h \
    $$PWD/combobox.h \
    $$PWD/countdowntimer.h \
    $$PWD/datetime.h \
    $$PWD/dialog.h \
    $$PWD/eventlistener.h \
    $$PWD/filedeleter.h \
    $$PWD/fontcombobox.h \
    $$PWD/hash.h \
    $$PWD/htmltag.h \
    $$PWD/network.h \
    $$PWD/ss.h \
    $$PWD/slider.h \
    $$PWD/stoppable.h \
    $$PWD/toolbutton.h \
    $$PWD/toolbuttonwithid.h \
    $$PWD/webdialog.h \
    $$PWD/withsizehint.h

SOURCES += \
    $$PWD/actionwithid.cc \
    $$PWD/combobox.cc \
    $$PWD/countdowntimer.cc \
    $$PWD/datetime.cc \
    $$PWD/dialog.cc \
    $$PWD/filedeleter.cc \
    $$PWD/fontcombobox.cc \
    $$PWD/slider.cc \
    $$PWD/toolbutton.cc \
    $$PWD/toolbuttonwithid.cc \
    $$PWD/webdialog.cc

QT +=   core gui

# EOF


