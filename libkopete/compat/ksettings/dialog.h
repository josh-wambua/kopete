/*  This file is part of the KDE project
    Copyright (C) 2003 Matthias Kretz <kretz@kde.org>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License version 2 as published by the Free Software Foundation.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.

*/

#ifndef KSETTINGS_DIALOG_H
#define KSETTINGS_DIALOG_H

#include <qobject.h>
#include <kservice.h>

template<class T> class QValueList;
class KPluginInfo;
class KCMultiDialog;
class KCModuleInfo;

namespace KSettings
{

/**
 * @short Generic configuration dialog that even works over component boundaries
 *
 * This class aims to standardize the use of configuration dialogs in KDE
 * applications. Especially when using KParts and/or Plugins you face problems
 * creating a consistent config dialog.
 *
 * To show a configuration dialog you only have to call the show method and be
 * done with it. A code example:
 *
 * You initialize \p m_cfgdlg with
 * \code
 * m_cfgdlg = new Dialog( Dialog::Static, this );
 * \endcode
 * If you use a KPart that was not especially designed for your app you can use
 * the second constructor:
 * \code
 * QStringList kpartslist;
 * for( all my kparts )
 *   kpartslist += m_mypart->instance().instanceName();
 * m_cfgdlg = new Dialog( kpartslist, this );
 * \endcode
 * and the action for the config dialog is connected to the show slot:
 * \code
 * KStdAction::preferences( m_cfgdlg, SLOT( show() ), actionCollection() );
 * \endcode
 *
 * If you need to be informed when the config was changed and applied in the
 * dialog you might want to take a look at Dispatcher.
 *
 * @author Matthias Kretz <kretz@kde.org>
 * @since 3.2
 */
class Dialog : public QObject
{
    Q_OBJECT
    public:
        /**
         * Tells the dialog whether the entries in the listview are all static
         * or whether it should add a Configure... button to select which parts
         * of the optional functionality should be active or not.
         */
        enum ContentInListView
        {
            /**
             * Static listview, while running no entries are added or deleted
             */
            Static,
            /**
             * Configurable listview. The user can select what functionality he
             * wants.
             */
            Configurable
        };

        /**
         * Construct a new Preferences Dialog for the application. It uses all
         * KCMs with X-KDE-ParentApp set to KGlobal::instance()->instanceName().
         *
         * @param parent       The parent is only used as the parent for the
         *                     dialog - centering the dialog over the parent
         *                     widget.
         * @param name         name
         */
        Dialog( QWidget * parent = 0, const char * name = 0 );

        /**
         * Construct a new Preferences Dialog for the application. It uses all
         * KCMs with X-KDE-ParentApp set to KGlobal::instance()->instanceName().
         *
         * @param content      Select whether you want a static or configurable
         *                     config dialog.
         * @param parent       The parent is only used as the parent for the
         *                     dialog - centering the dialog over the parent
         *                     widget.
         * @param name         name
         */
        Dialog( ContentInListView content = Static, QWidget * parent = 0,
                const char * name = 0 );

        /**
         * Construct a new Preferences Dialog with the pages for the selected
         * instance names. For example if you want to have the configuration
         * pages for the kviewviewer KPart you would pass a
         * QStringList consisting of only the name of the part "kviewviewer".
         *
         * @param components   A list of the names of the components that your
         *                     config dialog should merge the config pages in.
         * @param parent       The parent is only used as the parent for the
         *                     dialog - centering the dialog over the parent
         *                     widget.
         * @param name         name
         */
        Dialog( const QStringList & components, QWidget * parent = 0,
                const char * name = 0 );

        /**
         * Construct a new Preferences Dialog with the pages for the selected
         * instance names. For example if you want to have the configuration
         * pages for the kviewviewer KPart you would pass a
         * QStringList consisting of only the name of the part "kviewviewer".
         *
         * @param components   A list of the names of the components that your
         *                     config dialog should merge the config pages in.
         * @param content      Select whether you want a static or configurable
         *                     config dialog.
         * @param parent       The parent is only used as the parent for the
         *                     dialog - centering the dialog over the parent
         *                     widget.
         * @param name         name
         */
        Dialog( const QStringList & components, ContentInListView
                content, QWidget * parent = 0, const char * name = 0 );

        ~Dialog();

        /**
         * If you use a Configurable dialog you need to pass KPluginInfo
         * objects that the dialog should configure.
         */
        void addPluginInfos( const QValueList<KPluginInfo*> & plugininfos );

        KCMultiDialog * dialog();

    public slots:
        /**
         * Show the config dialog. The slot immediately returns since the dialog
         * is non-modal.
         */
        void show();

    protected slots:
        void configureTree();
        void updateTreeList();

    private:
        bool isPluginForKCMEnabled( KCModuleInfo * ) const;
        QValueList<KService::Ptr> instanceServices() const;
        QValueList<KService::Ptr> parentComponentsServices(
                const QStringList & ) const;
        void parseGroupFile( const QString & );
        void createDialogFromServices();
        class DialogPrivate;
        DialogPrivate * d;
};

}

// vim: sw=4 sts=4 et
#endif // KSETTINGS_DIALOG_H
