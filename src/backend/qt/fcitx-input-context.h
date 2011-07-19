#ifndef __FCITX_INPUT_CONTEXT_H_
#define __FCITX_INPUT_CONTEXT_H_

#include <QInputContext>
#include <QList>
#include <QDBusConnection>
#include <QDir>
#include "org.freedesktop.DBus.h"
#include "org.fcitx.Fcitx.InputMethod.h"
#include "org.fcitx.Fcitx.InputContext.h"


#define FCITX_IDENTIFIER_NAME "fcitx"

class FcitxInputContext : public QInputContext {
    Q_OBJECT
public:
    FcitxInputContext ();
    ~FcitxInputContext ();

    virtual QString identifierName();
    virtual QString language();
    virtual void reset();
    virtual bool isComposing() const;
    virtual void update();
    virtual void setFocusWidget( QWidget *w );

    virtual void widgetDestroyed(QWidget *w);

#if defined(Q_WS_X11)
    virtual bool x11FilterEvent( QWidget *keywidget, XEvent *event );
#endif // Q_WS_X11
    virtual bool filterEvent(const QEvent* event);
    
private Q_SLOTS:
    void imChanged(const QString& service, const QString& oldowner, const QString& newowner);
    void closeIM();
    void enableIM();
    void commitString(const QString& str);
    void forwardKey(uint keyval, uint state, int type);
private:
    void createInputContext();
#if defined(Q_WS_X11)
    XEvent* createXEvent(Display* dpy, WId wid, uint keyval, uint state, int type);
#endif // Q_WS_X11
    QKeyEvent* createKeyEvent(uint keyval, uint state, int type);
    bool isValid();
    
    QDBusConnection m_connection;
    org::freedesktop::DBus* m_dbusproxy;
    org::fcitx::Fcitx::InputMethod* m_improxy;
    org::fcitx::Fcitx::InputContext* m_icproxy;
    int m_id;
    QString m_path;
    bool m_enable;
    bool m_has_focus;
};

#endif //__FCITX_INPUT_CONTEXT_H_
