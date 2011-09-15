/*
 *  This file is part of KIMToy, an input method frontend for KDE
 *  Copyright (C) 2011 Ni Hui <shuizhuyuanluo@126.com>
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License as
 *  published by the Free Software Foundation; either version 2 of
 *  the License or (at your option) version 3 or any later version
 *  accepted by the membership of KDE e.V. (or its successor approved
 *  by the membership of KDE e.V.), which shall act as a proxy
 *  defined in Section 14 of version 3 of the license.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "statusbar.h"

#include <QAction>
#include <QDBusConnection>
#include <QMenu>
#include <QMouseEvent>
#include <QSignalMapper>

#include <KAboutApplicationDialog>
#include <KAction>
#include <KActionCollection>
#include <KApplication>
#include <KConfig>
#include <KConfigDialog>
#include <KConfigGroup>
#include <KDebug>
#include <KGlobal>
#include <KIcon>
#include <KLocale>
#include <KMenu>
#include <KMessageBox>
#include <KStandardAction>
#include <KStatusNotifierItem>
#include <KToggleAction>
#include <KWindowSystem>
#include <Plasma/Theme>

#include "impanelagent.h"
#include "propertywidget.h"
#include "preeditbar.h"
#include "statusbarlayout.h"
#include "themeragent.h"

#include "kimtoysettings.h"
#include "inputmethod.h"
#include "appearance.h"
#include "performance.h"

static void extractProperty( const QString& str,
                             QString& objectPath,
                             QString& name,
                             QString& iconName,
                             QString& description )
{
    const QStringList list = str.split( ':' );
    objectPath = list.at( 0 );
    name = list.at( 1 );
    iconName = list.at( 2 );
    description = list.at( 3 );
}

StatusBar::StatusBar()
{
    setWindowFlags( Qt::FramelessWindowHint | Qt::X11BypassWindowManagerHint );
    KWindowSystem::setState( winId(), NET::SkipTaskbar | NET::SkipPager | NET::StaysOnTop );
    KWindowSystem::setType( winId(), NET::PopupMenu );

    ThemerAgent::loadSettings();

    m_preeditBar = new PreEditBar;

    m_tray = new KStatusNotifierItem( this );
    m_tray->setAssociatedWidget( m_tray->contextMenu() );
    m_tray->setIconByName( "draw-freehand" );
    m_tray->setTitle( i18n( "KIMToy" ) );
    m_tray->setToolTipIconByName( "draw-freehand" );
    m_tray->setToolTipTitle( i18n( "KIMToy" ) );
    m_tray->setToolTipSubTitle( i18n( "Input method toy" ) );
    m_tray->setCategory( KStatusNotifierItem::ApplicationStatus );
    m_tray->setStatus( KStatusNotifierItem::Passive );

    KToggleAction* autostartAction = new KToggleAction( i18n( "A&utostart" ), this );
    autostartAction->setChecked( KIMToySettings::self()->autostartKIMToy() );
    connect( autostartAction, SIGNAL(toggled(bool)), this, SLOT(slotAutostartToggled(bool)) );
    m_tray->contextMenu()->addAction( autostartAction );

    KAction* prefAction = KStandardAction::preferences( this, SLOT(preferences()), 0 );
    m_tray->contextMenu()->addAction( prefAction );

    KAction* aboutAction = new KAction( KIcon( "draw-freehand" ), i18n( "&About KIMToy..." ), this );
    connect( aboutAction, SIGNAL(triggered()), this, SLOT(slotAboutActionTriggered()) );
    m_tray->contextMenu()->addAction( aboutAction );

    m_signalMapper = new QSignalMapper( this );
    connect( m_signalMapper, SIGNAL(mapped(const QString&)),
             this, SLOT(slotTriggerProperty(const QString&)) );

    m_layout = new StatusBarLayout;
    setLayout( m_layout );

//     m_hideButton = new QPushButton;
//     m_hideButton->installEventFilter( this );
//     m_hideButton->setFlat( true );
//     m_hideButton->setFixedSize( QSize( 22, 22 ) );
//     m_hideButton->setIcon( KIcon( "arrow-down-double" ) );
//     connect( m_hideButton, SIGNAL(clicked()),
//              this, SLOT(hide()) );
    bool enableTransparency = KIMToySettings::self()->backgroundTransparency();
    setAttribute( Qt::WA_TranslucentBackground, enableTransparency );
    m_preeditBar->setAttribute( Qt::WA_TranslucentBackground, enableTransparency );

    setAttribute( Qt::WA_AlwaysShowToolTips, true );

    installEventFilter( this );

    m_moving = false;

    QDBusConnection connection = QDBusConnection::sessionBus();
    connection.connect( "", "/kimpanel", "org.kde.kimpanel.inputmethod", "Enable",
                        this, SLOT(slotEnable(bool)) );
    connection.connect( "", "/kimpanel", "org.kde.kimpanel.inputmethod", "RegisterProperties",
                        this, SLOT(slotRegisterProperties(const QStringList&)) );
    connection.connect( "", "/kimpanel", "org.kde.kimpanel.inputmethod", "UpdateProperty",
                        this, SLOT(slotUpdateProperty(const QString&)) );
    connection.connect( "", "/kimpanel", "org.kde.kimpanel.inputmethod", "RemoveProperty",
                        this, SLOT(slotRemoveProperty(const QString&)) );
    connection.connect( "", "/kimpanel", "org.kde.kimpanel.inputmethod", "ExecDialog",
                        this, SLOT(slotExecDialog(const QString&)) );
    connection.connect( "", "/kimpanel", "org.kde.kimpanel.inputmethod", "ExecMenu",
                        this, SLOT(slotExecMenu(const QStringList&)) );

    KConfigGroup group( KGlobal::config(), "General" );
    QPoint pos = group.readEntry( "XYPosition", QPoint( 100, 0 ) );
    move( pos );

    loadSettings();

    Plasma::Theme* plasmaTheme = Plasma::Theme::defaultTheme();
    connect(plasmaTheme, SIGNAL(themeChanged()), this, SLOT(loadSettings()));

    IMPanelAgent::PanelCreated();
}

StatusBar::~StatusBar()
{
    KConfigGroup group( KGlobal::config(), "General" );
    group.writeEntry( "XYPosition", pos() );
    delete m_preeditBar;
    IMPanelAgent::Exit();
}

bool StatusBar::eventFilter( QObject* object, QEvent* event )
{
    if ( event->type() == QEvent::MouseButtonPress ) {
        QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
        if ( mouseEvent->button() == Qt::RightButton ) {
            QWidget* w = static_cast<QWidget*>(object);
            if ( w == this )
                m_pointPos = mouseEvent->pos();
            else
                m_pointPos = w->mapToParent( mouseEvent->pos() );
            m_moving = true;
            return true;
        }
        m_moving = false;
        return QObject::eventFilter( object, event );
    }
    if ( event->type() == QEvent::MouseMove && m_moving ) {
        QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
        move( mouseEvent->globalPos() - m_pointPos );
        return true;
    }
    return QObject::eventFilter( object, event );
}

void StatusBar::resizeEvent( QResizeEvent* event )
{
    ThemerAgent::resizeStatusBar( event->size() );
    if ( KIMToySettings::self()->enableWindowMask() ) {
        ThemerAgent::maskStatusBar( this );
    }
    if ( KIMToySettings::self()->enableBackgroundBlur() ) {
        ThemerAgent::blurStatusBar( this );
    }
}

void StatusBar::paintEvent( QPaintEvent* event )
{
    ThemerAgent::drawStatusBar( this );
}

void StatusBar::slotEnable( bool enable )
{
    setVisible( enable );
}

void StatusBar::slotTriggerProperty( const QString& objectPath )
{
//     kWarning() << "trigger property" << objectPath;
    IMPanelAgent::TriggerProperty( objectPath );
}

void StatusBar::slotRegisterProperties( const QStringList& props )
{
    QString objectPath, name, iconName, description;
    foreach ( const QString& p, props ) {
        extractProperty( p, objectPath, name, iconName, description );
        kWarning() << objectPath << name << iconName << description;
        int index = m_objectPaths.indexOf( objectPath );
        if ( index == -1 ) {
            /// no such objectPath, register it
            m_objectPaths << objectPath;
            PropertyWidget* pw = new PropertyWidget;
            pw->installEventFilter( this );
            connect( pw, SIGNAL(clicked()), m_signalMapper, SLOT(map()) );
            m_signalMapper->setMapping( pw, objectPath );
            m_layout->addWidget( pw );
            index = m_objectPaths.count() - 1;
        }

        /// update property
        PropertyWidget* w = static_cast<PropertyWidget*>(m_layout->itemAt( index )->widget());
        w->setProperty( name, iconName, description );

        if ( index == m_objectPaths.count() - 1 ) {
            /// update if new property just registered
            updateSize();
        }
    }
}

void StatusBar::slotUpdateProperty( const QString& prop )
{
    QString objectPath, name, iconName, description;
    extractProperty( prop, objectPath, name, iconName, description );
    kWarning() << objectPath << name << iconName << description;
    int index = m_objectPaths.indexOf( objectPath );
    if ( index == -1 ) {
        /// no such objectPath
        kWarning() << "update property without register it! " << objectPath;
        return;
    }

    /// update property
    PropertyWidget* w = static_cast<PropertyWidget*>(m_layout->itemAt( index )->widget());
    w->setProperty( name, iconName, description );
}

void StatusBar::slotRemoveProperty( const QString& prop )
{
    QString objectPath, name, iconName, description;
    extractProperty( prop, objectPath, name, iconName, description );
    int index = m_objectPaths.indexOf( objectPath );
    if ( index == -1 ) {
        /// no such objectPath
        kWarning() << "remove property without register it! " << objectPath;
        return;
    }

    /// remove property
    m_objectPaths.removeAt( index );
    delete m_layout->takeAt( index );
}

void StatusBar::slotExecDialog( const QString& prop )
{
    QString objectPath, name, iconName, description;
    extractProperty( prop, objectPath, name, iconName, description );
    KMessageBox::information( 0, description, name );
}

void StatusBar::slotExecMenu( const QStringList& actions )
{
    QMenu menu;
    QString objectPath, name, iconName, description;
    foreach ( const QString& a, actions ) {
        extractProperty( a, objectPath, name, iconName, description );
        QAction* action = new QAction( KIcon( iconName ), name, &menu );
        connect( action, SIGNAL(triggered()), m_signalMapper, SLOT(map()) );
        m_signalMapper->setMapping( action, objectPath );
        menu.addAction( action );
    }
    menu.exec( QCursor::pos() );
}

void StatusBar::slotAutostartToggled( bool enable )
{
    KIMToySettings::self()->setAutostartKIMToy( enable );
}

void StatusBar::preferences()
{
    if ( KConfigDialog::showDialog( "settings" ) )
        return;
    KConfigDialog* dialog = new KConfigDialog( this, "settings", KIMToySettings::self() );
    dialog->setFaceType( KPageDialog::List );
    dialog->addPage( new InputMethodWidget, i18n( "Input method" ), "draw-freehand" );
    dialog->addPage( new AppearanceWidget, i18n( "Appearance" ), "preferences-desktop-color" );
    dialog->addPage( new PerformanceWidget, i18n( "Performance" ), "preferences-system-performance" );
    connect( dialog, SIGNAL(settingsChanged(const QString&)),
             this, SLOT(loadSettings()) );
    dialog->show();
}

void StatusBar::slotAboutActionTriggered()
{
    KAboutApplicationDialog dlg( KGlobal::mainComponent().aboutData() );
    dlg.exec();
}

void StatusBar::loadSettings()
{
    ThemerAgent::loadSettings();
    ThemerAgent::loadTheme();

    bool enableTransparency = KIMToySettings::self()->backgroundTransparency();
    setAttribute( Qt::WA_TranslucentBackground, enableTransparency );
    m_preeditBar->setAttribute( Qt::WA_TranslucentBackground, enableTransparency );

    if ( KIMToySettings::self()->enableWindowMask() ) {
        ThemerAgent::maskStatusBar( this );
        ThemerAgent::maskPreEditBar( m_preeditBar );
    }
    else {
        clearMask();
        m_preeditBar->clearMask();
    }

    ThemerAgent::layoutStatusBar( m_layout );
    updateSize();
    m_preeditBar->resize( ThemerAgent::sizeHintPreEditBar( m_preeditBar ) );
}

void StatusBar::updateSize()
{
    resize( ThemerAgent::sizeHintStatusBar( this ) );
}
