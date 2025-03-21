#ifndef TEXTVIEWCTRL_H
#define TEXTVIEWCTRL_H

/*
* Copyright 2013-2025 Rochus Keller <mailto:me@rochus-keller.ch>
*
* This file is part of the Herald application.
*
* The following is the license that applies to this copy of the
* application. For a license to use the application under conditions
* other than those described here, please email to me@rochus-keller.ch.
*
* GNU General Public License Usage
* This file may be used under the terms of the GNU General Public
* License (GPL) versions 2.0 or 3.0 as published by the Free Software
* Foundation and appearing in the file LICENSE.GPL included in
* the packaging of this file. Please review the following information
* to ensure GNU General Public Licensing requirements will be met:
* http://www.fsf.org/licensing/licenses/info/GPLv2.html and
* http://www.gnu.org/copyleft/gpl.html.
*/

#include <QObject>
#include <Udb/Obj.h>
#include <GuiTools/AutoMenu.h>
#include <Udb/UpdateInfo.h>
#include <QLabel>

namespace Oln
{
    class OutlineUdbCtrl;
}
namespace He
{
    class ObjectTitleFrame;
    class CheckLabel;

    class TextViewCtrl : public QObject
    {
        Q_OBJECT
    public:
        explicit TextViewCtrl(QWidget *parent = 0);
        QWidget* getWidget() const;
		static TextViewCtrl* create(QWidget* parent, Udb::Transaction* txn );
        void setObj( const Udb::Obj& );
        Gui::AutoMenu* createPopup();
    signals:
        void signalFollowObject( const Udb::Obj& );
    protected slots:
        void onDbUpdate( Udb::UpdateInfo info );
        void onFollowAlias();
        void onTitleClick();
        void onAnchorActivated(QByteArray data, bool url);
    private:
        ObjectTitleFrame* d_title;
        Oln::OutlineUdbCtrl* d_oln;
        CheckLabel* d_pin;
    };

    class CheckLabel : public QLabel
    {
    public:
        CheckLabel( QWidget* w):QLabel(w),d_checked(false){}
        bool isChecked() const { return d_checked; }
        void setChecked( bool );
    protected:
        void mousePressEvent ( QMouseEvent * event );
    private:
        bool d_checked;
    };
}

#endif // TEXTVIEWCTRL_H
