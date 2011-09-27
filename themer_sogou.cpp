/*
 *  This file is part of KIMToy, an input method frontend for KDE
 *  Copyright (C) 2011 Ni Hui <shuizhuyuanluo@126.com>
 *
 *  This h_program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License as
 *  published by the Free Software Foundation; either version 2 of
 *  the License or (at your oh_ption) version 3 or any later version
 *  acceh_pted by the membership of KDE e.V. (or its successor approved
 *  by the membership of KDE e.V.), which shall act as a h_proxy
 *  defined in Section 14 of version 3 of the license.
 *
 *  This h_program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the imh_plied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this h_program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "themer_sogou.h"

#include <QBitmap>
#include <QColor>
#include <QFile>
#include <QFontMetrics>
#include <QPainter>
#include <QPixmap>
#include <QString>
#include <QTextStream>

#include <KDebug>
#include <KIconLoader>
#include <KZip>
#include <KZipFileEntry>
#include <Plasma/WindowEffects>

#include "preeditbar.h"
#include "statusbar.h"
#include "statusbarlayout.h"
#include "propertywidget.h"

#include "kimtoysettings.h"

static void calculateOverlaySurrounding( const QHash<QString, OverlayPixmap>& overlays, int& opt, int& opb, int& opl, int& opr )
{
    opl = 0;
    opr = 0;
    opt = 0;
    opb = 0;
    QHash<QString, OverlayPixmap>::ConstIterator it = overlays.constBegin();
    QHash<QString, OverlayPixmap>::ConstIterator end = overlays.constEnd();
    while ( it != end ) {
        const OverlayPixmap& op = it.value();
        switch ( op.alignArea ) {
            case 1:
                opl = qMax( opl, op.pixmap.width() );
                opt = qMax( opt, op.pixmap.height() );
                break;
            case 2:
                opt = qMax( opt, op.pixmap.height() );
                break;
            case 3:
                opr = qMax( opr, op.pixmap.width() );
                opt = qMax( opt, op.pixmap.height() );
                break;
            case 4:
                opl = qMax( opl, op.pixmap.width() );
                break;
            case 5:
                /// center pixmap, no addition
                break;
            case 6:
                opr = qMax( opr, op.pixmap.width() );
                break;
            case 7:
                opl = qMax( opl, op.pixmap.width() );
                opb = qMax( opb, op.pixmap.height() );
                break;
            case 8:
                opb = qMax( opb, op.pixmap.height() );
                break;
            case 9:
                opr = qMax( opr, op.pixmap.width() );
                opb = qMax( opb, op.pixmap.height() );
                break;
            default:
                /// never arrive here
                break;
        }
        ++it;
    }
}

/**
 *          |         |          |
 * ---------+---------+---------vst
 *          |         | ^vstm    |
 * ---------+---------+---------vsb
 *          |<--hstm->|          |
 * --------hsl-------hsr-------skinw-----skinh
 */

PreEditBarSkin::PreEditBarSkin()
{
    skinw = 0;
    skinh = 0;
}

PreEditBarSkin::PreEditBarSkin( const QPixmap& skinpix, int hsl, int hsr, int vst, int vsb )
{
    skinw = skinpix.width();
    skinh = skinpix.height();
    topleft = skinpix.copy( 0, 0, hsl, vst );
    top = skinpix.copy( hsl, 0, hsr - hsl, vst );
    topright = skinpix.copy( hsr, 0, skinw - hsr, vst );
    left = skinpix.copy( 0, vst, hsl, vsb - vst );
    center = skinpix.copy( hsl, vst, hsr - hsl, vsb - vst );
    right = skinpix.copy( hsr, vst, skinw - hsr, vsb - vst );
    bottomleft = skinpix.copy( 0, vsb, hsl, skinh - vsb );
    bottom = skinpix.copy( hsl, vsb, hsr - hsl, skinh - vsb );
    bottomright = skinpix.copy( hsr, vsb, skinw - hsr, skinh - vsb );
    topleftRegion = topleft.mask();
    topRegion = top.mask();
    toprightRegion = topright.mask();
    leftRegion = left.mask();
    centerRegion = center.mask();
    rightRegion = right.mask();
    bottomleftRegion = bottomleft.mask();
    bottomRegion = bottom.mask();
    bottomrightRegion = bottomright.mask();
}

ThemerSogou* ThemerSogou::m_self = 0;

ThemerSogou* ThemerSogou::self()
{
    if ( !m_self )
        m_self = new ThemerSogou;
    return m_self;
}

ThemerSogou::ThemerSogou()
: Themer()
{
}

ThemerSogou::~ThemerSogou()
{
}

bool ThemerSogou::loadTheme()
{
    QString file = KIMToySettings::self()->importTheme().path();
    if ( !QFile::exists( file ) )
        return false;

    KZip zip( file );
    if ( !zip.open( QIODevice::ReadOnly ) ) {
        return false;
    }

    const KArchiveEntry* entry = zip.directory()->entry( "skin.ini" );
    const KZipFileEntry* skinini = static_cast<const KZipFileEntry*>(entry);

    if ( !skinini ) {
        return false;
    }

    QByteArray data = skinini->data();

    /// parse ini file content
    bool general = false;
    bool display = false;
    bool scheme_h1 = false;
    bool scheme_v1 = false;
    bool statusbar = false;
    QPixmap h1skin;
    QPixmap v1skin;
    int fontPixelSize = 12;
    QString font_ch, font_en;
    QString pinyin_color, zhongwen_color;
    int i = 0;
    h_overlays.clear();
    v_overlays.clear();

    QTextStream ss( data );
    QString line;
    QString key, value;
    do {
        line = ss.readLine();
        if ( line.isEmpty() )
            continue;

        if ( line.at( 0 ) == '[' ) {
            general = ( line == "[General]" );
            display = ( line == "[Display]" );
            scheme_h1 = ( line == "[Scheme_H1]" );
            scheme_v1 = ( line == "[Scheme_V1]" );
            statusbar = ( line == "[StatusBar]" );
            continue;
        }

        key = line.split( '=' ).at( 0 );
        value = line.split( '=' ).at( 1 );

        if ( general ) {
            /// extract meta info
        }
        else if ( display ) {
            if ( key == "font_size" )
                fontPixelSize = value.trimmed().toInt();
            else if ( key == "font_ch" )
                font_ch = value;
            else if ( key == "font_en" )
                font_en = value;
            else if ( key == "pinyin_color" )
                pinyin_color = value;
            else if ( key == "zhongwen_color" )
                zhongwen_color = value;
        }
        else if ( scheme_h1 ) {
            if ( key == "pic" ) {
                const KArchiveEntry* e = zip.directory()->entry( value );
                const KZipFileEntry* pix = static_cast<const KZipFileEntry*>(e);
                if ( pix )
                    h1skin.loadFromData( pix->data() );
            }
            else if ( key == "layout_horizontal" ) {
                h_hstm = value.split( ',' ).at( 0 ).trimmed().toInt();
                h_hsl = value.split( ',' ).at( 1 ).trimmed().toInt();
                h_hsr = value.split( ',' ).at( 2 ).trimmed().toInt();
            }
            else if ( key == "layout_vertical" ) {
                h_vstm = value.split( ',' ).at( 0 ).trimmed().toInt();
                h_vst = value.split( ',' ).at( 1 ).trimmed().toInt();
                h_vsb = value.split( ',' ).at( 2 ).trimmed().toInt();
                //WARNING: it seems that v_vst/v_vsb is always unused in sogou theme
                h_vstm = 0;//WARNING: assume scale mode --- nihui
                h_vst = 1;
                h_vsb = 1;
            }
            else if ( key == "pinyin_marge" ) {
                QStringList list = value.split( ',' );
                h_pt = list.at( 0 ).trimmed().toInt();
                h_pb = list.at( 1 ).trimmed().toInt();
                h_pl = list.at( 2 ).trimmed().toInt();
                h_pr = list.at( 3 ).trimmed().toInt();
            }
            else if ( key == "zhongwen_marge" ) {
                QStringList list = value.split( ',' );
                h_zt = list.at( 0 ).trimmed().toInt();
                h_zb = list.at( 1 ).trimmed().toInt();
                h_zl = list.at( 2 ).trimmed().toInt();
                h_zr = list.at( 3 ).trimmed().toInt();
            }
            else if ( key.endsWith( "_display" ) ) {
                QString name = key.left( key.length() - 8 );
                h_overlays.insert( name, OverlayPixmap() );
            }
            else if ( key.endsWith( "_align" ) ) {
                QString name = key.left( key.length() - 6 );
                QStringList numbers = value.split( ',' );
                OverlayPixmap& op = h_overlays[ name ];
                op.mt = numbers.at( 0 ).toInt();
                op.mb = numbers.at( 1 ).toInt();
                op.ml = numbers.at( 2 ).toInt();
                op.mr = numbers.at( 3 ).toInt();
                op.alignVMode = numbers.at( 4 ).toInt() + numbers.at( 5 ).toInt();/// FIXME: right or wrong?
                op.alignHMode = numbers.at( 6 ).toInt() + numbers.at( 7 ).toInt();/// FIXME: right or wrong?
                op.alignArea = numbers.at( 8 ).toInt();
                op.alignTarget = numbers.at( 9 ).toInt();
            }
            else if ( h_overlays.contains( key ) ) {
                const KArchiveEntry* e = zip.directory()->entry( value );
                const KZipFileEntry* pix = static_cast<const KZipFileEntry*>(e);
                if ( pix )
                    h_overlays[ key ].pixmap.loadFromData( pix->data() );
            }
        }
        else if ( scheme_v1 ) {
            if ( key == "pic" ) {
                const KArchiveEntry* e = zip.directory()->entry( value );
                const KZipFileEntry* pix = static_cast<const KZipFileEntry*>(e);
                if ( pix )
                    v1skin.loadFromData( pix->data() );
            }
            else if ( key == "layout_horizontal" ) {
                v_hstm = value.split( ',' ).at( 0 ).trimmed().toInt();
                v_hsl = value.split( ',' ).at( 1 ).trimmed().toInt();
                v_hsr = value.split( ',' ).at( 2 ).trimmed().toInt();
            }
            else if ( key == "layout_vertical" ) {
                v_vstm = value.split( ',' ).at( 0 ).trimmed().toInt();
                v_vst = value.split( ',' ).at( 1 ).trimmed().toInt();
                v_vsb = value.split( ',' ).at( 2 ).trimmed().toInt();
            }
            else if ( key == "pinyin_marge" ) {
                QStringList list = value.split( ',' );
                v_pt = list.at( 0 ).trimmed().toInt();
                v_pb = list.at( 1 ).trimmed().toInt();
                v_pl = list.at( 2 ).trimmed().toInt();
                v_pr = list.at( 3 ).trimmed().toInt();
            }
            else if ( key == "zhongwen_marge" ) {
                QStringList list = value.split( ',' );
                v_zt = list.at( 0 ).trimmed().toInt();
                v_zb = list.at( 1 ).trimmed().toInt();
                v_zl = list.at( 2 ).trimmed().toInt();
                v_zr = list.at( 3 ).trimmed().toInt();
            }
            else if ( key.endsWith( "_display" ) ) {
                QString name = key.left( key.length() - 8 );
                v_overlays.insert( name, OverlayPixmap() );
            }
            else if ( key.endsWith( "_align" ) ) {
                QString name = key.left( key.length() - 6 );
                QStringList numbers = value.split( ',' );
                OverlayPixmap& op = v_overlays[ name ];
                op.mt = numbers.at( 0 ).toInt();
                op.mb = numbers.at( 1 ).toInt();
                op.ml = numbers.at( 2 ).toInt();
                op.mr = numbers.at( 3 ).toInt();
                op.alignVMode = numbers.at( 4 ).toInt() + numbers.at( 5 ).toInt();/// FIXME: right or wrong?
                op.alignHMode = numbers.at( 6 ).toInt() + numbers.at( 7 ).toInt();/// FIXME: right or wrong?
                op.alignArea = numbers.at( 8 ).toInt();
                op.alignTarget = numbers.at( 9 ).toInt();
            }
            else if ( v_overlays.contains( key ) ) {
                const KArchiveEntry* e = zip.directory()->entry( value );
                const KZipFileEntry* pix = static_cast<const KZipFileEntry*>(e);
                if ( pix )
                    v_overlays[ key ].pixmap.loadFromData( pix->data() );
            }
        }
        else if ( statusbar ) {
            if ( key == "pic" ) {
                const KArchiveEntry* e = zip.directory()->entry( value );
                const KZipFileEntry* pix = static_cast<const KZipFileEntry*>(e);
                if ( pix )
                    m_statusBarSkin.loadFromData( pix->data() );
            }
            else if ( key.endsWith( "_pos" ) ) {
                QStringList list = value.split( ',' );
                int x = list.at( 0 ).trimmed().toInt();
                int y = list.at( 1 ).trimmed().toInt();
                if ( x != 0 && y != 0 && i < 6 ) {
                    m_pwpos[ i ] = QPoint( x, y );
                    ++i;
                }
            }
        }
    } while ( !line.isNull() );

    h_hsr = h1skin.width() - h_hsr;
    h_vsb = h1skin.height() - h_vsb;
    v_hsr = v1skin.width() - v_hsr;
    v_vsb = v1skin.height() - v_vsb;
    if ( h_hsl > h_hsr ) qSwap( h_hsl, h_hsr );
    if ( h_vst > h_vsb ) qSwap( h_vst, h_vsb );
    if ( v_hsl > v_hsr ) qSwap( v_hsl, v_hsr );
    if ( v_vst > v_vsb ) qSwap( v_vst, v_vsb );

    h_preEditBarSkin = PreEditBarSkin( h1skin, h_hsl, h_hsr, h_vst, h_vsb );
    v_preEditBarSkin = PreEditBarSkin( v1skin, v_hsl, v_hsr, v_vst, v_vsb );

    /// calculate overlay pixmap surrounding size
    calculateOverlaySurrounding( h_overlays, h_opt, h_opb, h_opl, h_opr );
    calculateOverlaySurrounding( v_overlays, v_opt, v_opb, v_opl, v_opr );

    m_preEditFont.setFamily( font_en );
    m_preEditFont.setPixelSize( fontPixelSize );
    m_preEditFont.setBold( true );
    m_candidateFont.setFamily( font_ch );
    m_candidateFont.setPixelSize( fontPixelSize );
    m_candidateFont.setBold( true );
    m_labelFont = m_candidateFont;

    m_preEditFontHeight = QFontMetrics( m_preEditFont ).height();
    m_labelFontHeight = QFontMetrics( m_labelFont ).height();
    m_candidateFontHeight = QFontMetrics( m_candidateFont ).height();

    pinyin_color = pinyin_color.leftJustified( 8, '0' ).replace( "0x", "#" );
    zhongwen_color = zhongwen_color.leftJustified( 8, '0' ).replace( "0x", "#" );
    m_preEditColor = QColor( pinyin_color );
    m_candidateColor = QColor( zhongwen_color );
    m_labelColor = m_candidateColor;

    return true;
}

QSize ThemerSogou::sizeHintPreEditBar( const PreEditBar* widget ) const
{
    const PreEditBarSkin& skin = KIMToySettings::self()->verticalPreeditBar()
                            ? v_preEditBarSkin : h_preEditBarSkin;
    int w = skin.skinw;
    int h = skin.skinh;

    if ( KIMToySettings::self()->verticalPreeditBar() ) {
        int widgetsh = v_pt + v_pb + v_zt + v_zb;

        /// preedit and aux
        int pinyinauxw = QFontMetrics( m_preEditFont ).width( widget->m_text + widget->m_auxText );
        w = qMax( pinyinauxw + v_pl + v_pr + v_opl + v_opr, w );
        widgetsh += m_preEditFontHeight;

        /// lookuptable
        int lookuptablew = 0;
        int count = qMin( widget->m_labels.count(), widget->m_candidates.count() );
        for ( int i = 0; i < count; ++i ) {
            QString tmp = widget->m_labels.at( i ).trimmed() + widget->m_candidates.at( i ).trimmed();
            lookuptablew = qMax( QFontMetrics( m_candidateFont ).width( tmp ), lookuptablew );
            widgetsh += m_candidateFontHeight;
        }
        w = qMax( lookuptablew + v_zl + v_zr + v_opl + v_opr, w );

        h = qMax( widgetsh + v_opt + v_opb, h );
    }
    else {
        int widgetsh = h_pt + h_pb + h_zt + h_zb;

        /// preedit and aux
        int pinyinauxw = QFontMetrics( m_preEditFont ).width( widget->m_text + widget->m_auxText );
        w = qMax( pinyinauxw + h_pl + h_pr + h_opl + h_opr, w );
        widgetsh += m_preEditFontHeight;

        /// lookuptable
        QString tmp = widget->m_labels.join( QString() );
        int count = qMin( widget->m_labels.count(), widget->m_candidates.count() );
        for ( int i = 0; i < count; ++i ) {
            tmp += widget->m_labels.at( i ).trimmed() + widget->m_candidates.at( i ).trimmed();
        }
        int lookuptablew = QFontMetrics( m_candidateFont ).width( tmp );
        w = qMax( lookuptablew + h_zl + h_zr + h_opl + h_opr, w );
        widgetsh += m_candidateFontHeight;

        h = qMax( widgetsh + h_opt + h_opb, h );
    }

    if ( !KIMToySettings::self()->enablePreeditResizing() ) {
        /// align with skin width + 70 * x
        const int align = 70;
        if ( w > skin.skinw ) {
            w = skin.skinw + ( ( w - skin.skinw - 1 ) / align + 1 ) * align;
        }
    }

    return QSize( w, h );
}

QSize ThemerSogou::sizeHintStatusBar( const StatusBar* widget ) const
{
    return m_statusBarSkin.size();
}

void ThemerSogou::layoutStatusBar( StatusBarLayout* layout ) const
{
    int availableCount = qMin( layout->count(), 6 );
    for ( int i = 0; i < availableCount; ++i ) {
        QLayoutItem* item = layout->m_items.at( i );
        item->setGeometry( QRect( m_pwpos[ i ], item->maximumSize() ) );
    }
}

void ThemerSogou::resizePreEditBar( const QSize& size )
{
    PreEditBarSkin& skin = KIMToySettings::self()->verticalPreeditBar()
                        ? v_preEditBarSkin : h_preEditBarSkin;
    int hstm, vstm;

    if ( KIMToySettings::self()->verticalPreeditBar() ) {
        hstm = v_hstm;
        vstm = v_vstm;
    }
    else {
        hstm = h_hstm;
        vstm = h_vstm;
    }

    /**
        *          |         |          |
        * ---------+---------+---------vst
        *          |         | ^vstm    |
        * ---------+---------+---------vsb
        *          |<--hstm->|          |
        * --------hsl-------hsr-------skinw-----skinh
        */

    const int leftrightheight = size.height() - skin.topleft.height() - skin.bottomleft.height();
    const int topbottomwidth = size.width() - skin.topleft.width() - skin.topright.width();

    /// corners

    /// edges
    if ( hstm == 0 ) {
        /// scale
        if ( skin.top.width() != topbottomwidth ) {
            skin.top = skin.top.scaled( topbottomwidth, skin.top.height() );
            skin.bottom = skin.bottom.scaled( topbottomwidth, skin.bottom.height() );
            skin.topRegion = skin.top.mask();
            skin.bottomRegion = skin.bottom.mask();
        }
    }
    if ( vstm == 0 ) {
        /// scale
        if ( skin.left.height() != leftrightheight ) {
            skin.left = skin.left.scaled( skin.left.width(), leftrightheight );
            skin.right = skin.right.scaled( skin.right.width(), leftrightheight );
            skin.leftRegion = skin.left.mask();
            skin.rightRegion = skin.right.mask();
        }
    }

    /// center
    if ( hstm == 0 ) {
        /// scale
        if ( vstm == 0 ) {
            /// scale
            if ( skin.center.width() != topbottomwidth || skin.center.height() != leftrightheight ) {
                skin.center = skin.center.scaled( topbottomwidth, leftrightheight );
                skin.centerRegion = skin.center.mask();
            }
        }
        else {
            /// tilling
            if ( skin.center.width() != topbottomwidth ) {
                skin.center = skin.center.scaled( topbottomwidth, skin.center.height() );
                skin.centerRegion = skin.center.mask();
            }
        }
    }
    else {
        /// tilling
        if ( vstm == 0 ) {
            /// scale
            if ( skin.center.height() != leftrightheight ) {
                skin.center = skin.center.scaled( skin.center.width(), leftrightheight );
                skin.centerRegion = skin.center.mask();
            }
        }
    }

    /// calculate mask if necessary
    if ( KIMToySettings::self()->enableWindowMask()
        || KIMToySettings::self()->enableBackgroundBlur()
        || KIMToySettings::self()->backgroundColorizing() ) {
        updatePreEditBarMask( size );
    }
}

void ThemerSogou::resizeStatusBar( const QSize& size )
{
    /// calculate mask if necessary
    if ( KIMToySettings::self()->enableWindowMask()
        || KIMToySettings::self()->enableBackgroundBlur()
        || KIMToySettings::self()->backgroundColorizing() ) {
        updateStatusBarMask( size );
    }
}

void ThemerSogou::updatePreEditBarMask( const QSize& size )
{
    PreEditBarSkin& skin = KIMToySettings::self()->verticalPreeditBar()
                        ? v_preEditBarSkin : h_preEditBarSkin;
    int hstm, hsl, hsr, vstm, vst, vsb;
    int opt, opb, opl, opr;

    if ( KIMToySettings::self()->verticalPreeditBar() ) {
        hstm = v_hstm;
        hsl = v_hsl, hsr = v_hsr;
        vstm = v_vstm;
        vst = v_vst, vsb = v_vsb;
        opt = v_opt, opb = v_opb, opl = v_opl, opr = v_opr;
    }
    else {
        hstm = h_hstm;
        hsl = h_hsl, hsr = h_hsr;
        vstm = h_vstm;
        vst = h_vst, vsb = h_vsb;
        opt = h_opt, opb = h_opb, opl = h_opl, opr = h_opr;
    }

    /**
        *          |         |          |
        * ---------+---------+---------vst
        *          |         | ^vstm    |
        * ---------+---------+---------vsb
        *          |<--hstm->|          |
        * --------hsl-------hsr-------skinw-----skinh
        */

    const int middlepixh = vsb - vst;
    const int middlepixw = hsr - hsl;

    const int leftrightheight = size.height() - skin.topleft.height() - skin.bottomleft.height();
    const int topbottomwidth = size.width() - skin.topleft.width() - skin.topright.width();

    /// corners
    QRegion topleft = skin.topleftRegion;
    QRegion topright = skin.toprightRegion;
    topright.translate( hsl + topbottomwidth, 0 );
    QRegion bottomleft = skin.bottomleftRegion;
    bottomleft.translate( 0, vst + leftrightheight );
    QRegion bottomright = skin.bottomrightRegion;
    bottomright.translate( hsl + topbottomwidth, vst + leftrightheight );

    /// edges
    QRegion left, top, right, bottom;
    if ( hstm == 0 ) {
        /// scale
        top = skin.topRegion;
        bottom = skin.bottomRegion;
    }
    else {
        /// tilling
        top = QRegion( QRect( 0, 0, topbottomwidth, vst ) );
        bottom = QRegion( QRect( 0, 0, topbottomwidth, skin.bottom.height() ) );
        QRegion toppixRegion( skin.topRegion );
        QRegion bottompixRegion( skin.bottomRegion );
        QRegion tmpRegion = toppixRegion;
        QRegion tmp2Region = bottompixRegion;
        for ( int i = 0; i < topbottomwidth; i += middlepixw ) {
            toppixRegion |= tmpRegion;
            bottompixRegion |= tmp2Region;
            tmpRegion.translate( middlepixw, 0 );
            tmp2Region.translate( middlepixw, 0 );
        }
        top &= toppixRegion;
        bottom &= bottompixRegion;
    }
    if ( vstm == 0 ) {
        /// scale
        left = skin.leftRegion;
        right = skin.rightRegion;
    }
    else {
        /// tilling
        left = QRegion( QRect( 0, 0, hsl, leftrightheight ) );
        right = QRegion( QRect( 0, 0, skin.right.width(), leftrightheight ) );
        QRegion leftpixRegion( skin.leftRegion );
        QRegion rightpixRegion( skin.rightRegion );
        QRegion tmpRegion = leftpixRegion;
        QRegion tmp2Region = rightpixRegion;
        for ( int i = 0; i < leftrightheight; i += middlepixh ) {
            leftpixRegion |= tmpRegion;
            rightpixRegion |= tmp2Region;
            tmpRegion.translate( 0, middlepixh );
            tmp2Region.translate( 0, middlepixh );
        }
        left &= leftpixRegion;
        right &= rightpixRegion;
    }
    left.translate( 0, vst );
    top.translate( hsl, 0 );
    right.translate( hsl + topbottomwidth, vst );
    bottom.translate( hsl, vst + leftrightheight );

    /// center
    QRegion center( QRect( 0, 0, topbottomwidth, leftrightheight ) );
    if ( hstm == 0 ) {
        /// scale
        if ( vstm == 0 ) {
            /// scale
            center = skin.centerRegion;
        }
        else {
            /// tilling
            QRegion centerpixRegion( skin.centerRegion );
            QRegion tmpRegion = centerpixRegion;
            int i;
            for ( i = 0; i < leftrightheight; i += middlepixh ) {
                centerpixRegion |= tmpRegion;
                tmpRegion.translate( 0, middlepixh );
            }
            center &= centerpixRegion;
        }
    }
    else {
        /// tilling
        if ( vstm == 0 ) {
            /// scale
            QRegion centerpixRegion( skin.centerRegion );
            QRegion tmpRegion = centerpixRegion;
            int i;
            for ( i = 0; i < topbottomwidth; i += middlepixw ) {
                centerpixRegion |= tmpRegion;
                tmpRegion.translate( middlepixw, 0 );
            }
            center &= centerpixRegion;
        }
        else {
            /// tilling
            QRegion centerpixRegion( skin.centerRegion );
            QRegion tmpRegion = centerpixRegion;
            int i, j;
            for ( i = 0; i < topbottomwidth; i += middlepixw ) {
                QRegion tmp2Region = tmpRegion;
                for ( j = 0; j < leftrightheight; j += middlepixh ) {
                    centerpixRegion |= tmp2Region;
                    tmp2Region.translate( 0, middlepixh );
                }
                tmpRegion.translate( middlepixw, 0 );
            }
            center &= centerpixRegion;
        }
    }
    center.translate( hsl, vst );

    m_preEditBarMask = topleft | top | topright | left | center | right | bottomleft | bottom | bottomright;

    /// overlay pixmap regions
    const QHash<QString, OverlayPixmap>& overlays = KIMToySettings::self()->verticalPreeditBar()
                                                    ? v_overlays : h_overlays;
    QHash<QString, OverlayPixmap>::ConstIterator it = overlays.constBegin();
    QHash<QString, OverlayPixmap>::ConstIterator end = overlays.constEnd();
    while ( it != end ) {
        const OverlayPixmap& op = it.value();
        QRegion opRegion = op.pixmap.mask();
        switch ( op.alignArea ) {
            case 1:
                opRegion.translate( -op.mr, -op.mb );
                break;
            case 2:
                if ( op.alignHMode == 0 ) {
                    opRegion.translate( ( size.width() + opl - opr + op.pixmap.width() ) / 2, 0 );
                    opRegion.translate( 0, -op.mb );
                }
                else if ( op.alignHMode == 1 ) {
                    opRegion.translate( opl, 0 );
                    opRegion.translate( op.ml, -op.mb );
                }
                else if ( op.alignHMode == 2 ) {
                    opRegion.translate( size.width() - opr - op.pixmap.width(), 0 );
                    opRegion.translate( -op.mr, -op.mb );
                }
                break;
            case 3:
                opRegion.translate( size.width() - opr, 0 );
                opRegion.translate( op.ml, -op.mb );
                break;
            case 4:
                if ( op.alignVMode == 0 ) {
                    opRegion.translate( 0, ( size.height() - opb + opt + op.pixmap.height() ) / 2 );
                    opRegion.translate( -op.mr, 0 );
                }
                else if ( op.alignVMode == 1 ) {
                    opRegion.translate( 0, opt );
                    opRegion.translate( -op.mr, op.mt );
                }
                else if ( op.alignVMode == 2 ) {
                    opRegion.translate( 0, size.height() - opb - op.pixmap.height() );
                    opRegion.translate( -op.mr, -op.mb );
                }
                break;
            case 5:
                if ( op.alignHMode == 0 ) {
                    opRegion.translate( ( size.width() + opl - opr + op.pixmap.width() ) / 2, 0 );
                }
                else if ( op.alignHMode == 1 ) {
                    opRegion.translate( opl, 0 );
                    opRegion.translate( op.ml, 0 );
                }
                else if ( op.alignHMode == 2 ) {
                    opRegion.translate( size.width() - opr - op.pixmap.width(), 0 );
                    opRegion.translate( -op.mr, 0 );
                }
                if ( op.alignVMode == 0 ) {
                    opRegion.translate( 0, ( size.height() - opb + opt + op.pixmap.height() ) / 2 );
                }
                else if ( op.alignVMode == 1 ) {
                    opRegion.translate( 0, opt );
                    opRegion.translate( 0, op.mt );
                }
                else if ( op.alignVMode == 2 ) {
                    opRegion.translate( 0, size.height() - opb - op.pixmap.height() );
                    opRegion.translate( 0, -op.mb );
                }
                break;
            case 6:
                if ( op.alignVMode == 0 ) {
                    opRegion.translate( size.width() - opr, ( size.height() - opb + opt + op.pixmap.height() ) / 2 );
                    opRegion.translate( op.ml, 0 );
                }
                else if ( op.alignVMode == 1 ) {
                    opRegion.translate( size.width() - opr, opt );
                    opRegion.translate( op.ml, op.mt );
                }
                else if ( op.alignVMode == 2 ) {
                    opRegion.translate( size.width() - opr, size.height() - opb - op.pixmap.height() );
                    opRegion.translate( op.ml, -op.mb );
                }
                break;
            case 7:
                opRegion.translate( 0, size.height() - opb );
                opRegion.translate( -op.mr, op.mt );
                break;
            case 8:
                if ( op.alignHMode == 0 ) {
                    opRegion.translate( ( size.width() + opl - opr + op.pixmap.width() ) / 2, size.height() - opb );
                    opRegion.translate( 0, op.mt );
                }
                else if ( op.alignHMode == 1 ) {
                    opRegion.translate( opl, size.height() - opb );
                    opRegion.translate( op.ml, op.mt );
                }
                else if ( op.alignHMode == 2 ) {
                    opRegion.translate( size.width() - opr - op.pixmap.width(), size.height() - opb );
                    opRegion.translate( -op.mr, op.mt );
                }
                break;
            case 9:
                opRegion.translate( size.width() - opr, size.height() - opb );
                opRegion.translate( op.ml, op.mt );
                break;
            default:
                /// never arrive here
                break;
        }
        m_preEditBarMask |= opRegion;
        ++it;
    }
}

void ThemerSogou::updateStatusBarMask( const QSize& size )
{
    m_statusBarSkin = m_statusBarSkin.scaled( size );
    m_statusBarMask = m_statusBarSkin.mask();
}

void ThemerSogou::maskPreEditBar( PreEditBar* widget )
{
    widget->setMask( m_preEditBarMask );
}

void ThemerSogou::maskStatusBar( StatusBar* widget )
{
    QRegion mask = m_statusBarMask;
    foreach ( const QLayoutItem* item, widget->m_layout->m_items ) {
        mask |= item->geometry();
    }
    widget->setMask( mask );
}

void ThemerSogou::blurPreEditBar( PreEditBar* widget )
{
    Plasma::WindowEffects::enableBlurBehind( widget->winId(), true, m_preEditBarMask );
}

void ThemerSogou::blurStatusBar( StatusBar* widget )
{
    Plasma::WindowEffects::enableBlurBehind( widget->winId(), true, m_statusBarMask );
}

void ThemerSogou::drawPreEditBar( PreEditBar* widget )
{
//     kWarning() << "###" << widget->size();
    const PreEditBarSkin& skin = KIMToySettings::self()->verticalPreeditBar()
                        ? v_preEditBarSkin : h_preEditBarSkin;

    int pt = 0, pb = 0, pl = 0, pr = 0;
    int zt = 0, zb = 0, zl = 0, zr = 0;
    int opt = 0, opb = 0, opl = 0, opr = 0;

    QPainter p( widget );

    if ( KIMToySettings::self()->backgroundColorizing() ) {
        QPainterPath path;
        path.addRegion( m_preEditBarMask );
        p.fillPath( path, KIMToySettings::self()->preeditBarColorize() );
    }

    int hstm, hsl, vstm, vst;
    if ( KIMToySettings::self()->verticalPreeditBar() ) {
        hstm = v_hstm;
        hsl = v_hsl;
        vstm = v_vstm;
        vst = v_vst;
        pt = v_pt, pb = v_pb, pl = v_pl, pr = v_pr;
        zt = v_zt, zb = v_zb, zl = v_zl, zr = v_zr;
        opt = v_opt, opb = v_opb, opl = v_opl, opr = v_opr;
    }
    else {
        hstm = h_hstm;
        hsl = h_hsl;
        vstm = h_vstm;
        vst = h_vst;
        pt = h_pt, pb = h_pb, pl = h_pl, pr = h_pr;
        zt = h_zt, zb = h_zb, zl = h_zl, zr = h_zr;
        opt = h_opt, opb = h_opb, opl = h_opl, opr = h_opr;
    }

    /**
        *          |         |          |
        * ---------+---------+---------vst
        *          |         | ^vstm    |
        * ---------+---------+---------vsb
        *          |<--hstm->|          |
        * --------hsl-------hsr-------skinw-----skinh
        */

    const int leftrightheight = widget->height() - skin.topleft.height() - skin.bottomleft.height();
    const int topbottomwidth = widget->width() - skin.topleft.width() - skin.topright.width();

    /// corners
    p.drawPixmap( 0, 0, skin.topleft );
    p.drawPixmap( hsl + topbottomwidth, 0, skin.topright );
    p.drawPixmap( 0, vst + leftrightheight, skin.bottomleft );
    p.drawPixmap( hsl + topbottomwidth, vst + leftrightheight, skin.bottomright );

    /// edges
    if ( hstm == 0 ) {
        /// scale
        p.drawPixmap( hsl, 0, skin.top );
        p.drawPixmap( hsl, vst + leftrightheight, skin.bottom );
    }
    else {
        /// tilling
        p.drawTiledPixmap( hsl, 0, topbottomwidth, vst, skin.top );
        p.drawTiledPixmap( hsl, vst + leftrightheight, topbottomwidth, skin.bottom.height(), skin.bottom );
    }
    if ( vstm == 0 ) {
        /// scale
        p.drawPixmap( 0, vst, skin.left );
        p.drawPixmap( hsl + topbottomwidth, vst, skin.right );
    }
    else {
        /// tilling
        p.drawTiledPixmap( 0, vst, hsl, leftrightheight, skin.left );
        p.drawTiledPixmap( hsl + topbottomwidth, vst, skin.right.width(), leftrightheight, skin.right );
    }

    /// center
    if ( hstm == 0 && vstm == 0 ) {
        /// scale
        p.drawPixmap( hsl, vst, skin.center );
    }
    else {
        /// tilling
        p.drawTiledPixmap( hsl, vst, topbottomwidth, leftrightheight, skin.center );
    }

    /// draw overlay pixmap
    const QHash<QString, OverlayPixmap>& overlays = KIMToySettings::self()->verticalPreeditBar()
                                                    ? v_overlays : h_overlays;
    QHash<QString, OverlayPixmap>::ConstIterator it = overlays.constBegin();
    QHash<QString, OverlayPixmap>::ConstIterator end = overlays.constEnd();
    while ( it != end ) {
        const OverlayPixmap& op = it.value();
        p.save();
        switch ( op.alignArea ) {
            case 1:
                p.translate( -op.mr, -op.mb );
                break;
            case 2:
                if ( op.alignHMode == 0 ) {
                    p.translate( ( widget->width() + opl - opr + op.pixmap.width() ) / 2, 0 );
                    p.translate( 0, -op.mb );
                }
                else if ( op.alignHMode == 1 ) {
                    p.translate( opl, 0 );
                    p.translate( op.ml, -op.mb );
                }
                else if ( op.alignHMode == 2 ) {
                    p.translate( widget->width() - opr - op.pixmap.width(), 0 );
                    p.translate( -op.mr, -op.mb );
                }
                break;
            case 3:
                p.translate( widget->width() - opr, 0 );
                p.translate( op.ml, -op.mb );
                break;
            case 4:
                if ( op.alignVMode == 0 ) {
                    p.translate( 0, ( widget->height() - opb + opt + op.pixmap.height() ) / 2 );
                    p.translate( -op.mr, 0 );
                }
                else if ( op.alignVMode == 1 ) {
                    p.translate( 0, opt );
                    p.translate( -op.mr, op.mt );
                }
                else if ( op.alignVMode == 2 ) {
                    p.translate( 0, widget->height() - opb - op.pixmap.height() );
                    p.translate( -op.mr, -op.mb );
                }
                break;
            case 5:
                if ( op.alignHMode == 0 ) {
                    p.translate( ( widget->width() + opl - opr + op.pixmap.width() ) / 2, 0 );
                }
                else if ( op.alignHMode == 1 ) {
                    p.translate( opl, 0 );
                    p.translate( op.ml, 0 );
                }
                else if ( op.alignHMode == 2 ) {
                    p.translate( widget->width() - opr - op.pixmap.width(), 0 );
                    p.translate( -op.mr, 0 );
                }
                if ( op.alignVMode == 0 ) {
                    p.translate( 0, ( widget->height() - opb + opt + op.pixmap.height() ) / 2 );
                }
                else if ( op.alignVMode == 1 ) {
                    p.translate( 0, opt );
                    p.translate( 0, op.mt );
                }
                else if ( op.alignVMode == 2 ) {
                    p.translate( 0, widget->height() - opb - op.pixmap.height() );
                    p.translate( 0, -op.mb );
                }
                break;
            case 6:
                if ( op.alignVMode == 0 ) {
                    p.translate( widget->width() - opr, ( widget->height() - opb + opt + op.pixmap.height() ) / 2 );
                    p.translate( op.ml, 0 );
                }
                else if ( op.alignVMode == 1 ) {
                    p.translate( widget->width() - opr, opt );
                    p.translate( op.ml, op.mt );
                }
                else if ( op.alignVMode == 2 ) {
                    p.translate( widget->width() - opr, widget->height() - opb - op.pixmap.height() );
                    p.translate( op.ml, -op.mb );
                }
                break;
            case 7:
                p.translate( 0, widget->height() - opb );
                p.translate( -op.mr, op.mt );
                break;
            case 8:
                if ( op.alignHMode == 0 ) {
                    p.translate( ( widget->width() + opl - opr + op.pixmap.width() ) / 2, widget->height() - opb );
                    p.translate( 0, op.mt );
                }
                else if ( op.alignHMode == 1 ) {
                    p.translate( opl, widget->height() - opb );
                    p.translate( op.ml, op.mt );
                }
                else if ( op.alignHMode == 2 ) {
                    p.translate( widget->width() - opr - op.pixmap.width(), widget->height() - opb );
                    p.translate( -op.mr, op.mt );
                }
                break;
            case 9:
                p.translate( widget->width() - opr, widget->height() - opb );
                p.translate( op.ml, op.mt );
                break;
            default:
                /// never arrive here
                break;
        }
        p.drawPixmap( 0, 0, op.pixmap );
        p.restore();
        ++it;
    }


    p.translate( opl, opt );
    int y = 0;

    y += pt;
    if ( widget->preeditVisible || widget->auxVisible ) {
        /// draw preedit / aux text
        p.save();
        p.setFont( m_preEditFont );
        p.setPen( m_preEditColor );

        p.drawText( pl, pt, widget->width() - pl - pr, m_preEditFontHeight, Qt::AlignLeft, widget->m_text + widget->m_auxText );
        if ( widget->preeditVisible ) {
            int pixelsWide = QFontMetrics( m_preEditFont ).width( widget->m_text.left( widget->m_cursorPos ) );
            p.drawLine( pl + pixelsWide, pt, pl + pixelsWide, pt + m_preEditFontHeight );
        }
        p.restore();
    }
    /// always preserve space when theme enabled
    y += m_preEditFontHeight + pb;


    if ( widget->lookuptableVisible ) {
        /// draw lookup table
        int x = zl;
        y += zt;
        int w = 0;
        int h = qMax( m_labelFontHeight, m_candidateFontHeight );

        /// draw labels and candidates
        int count = qMin( widget->m_labels.count(), widget->m_candidates.count() );

        if ( KIMToySettings::self()->verticalPreeditBar() ) {
            for ( int i = 0; i < count; ++i ) {
                /// draw label
                p.setFont( m_labelFont );
                p.setPen( m_labelColor );
                x = zl;
                w = p.fontMetrics().width( widget->m_labels.at( i ) );
                p.drawText( x, y, w, h, Qt::AlignCenter, widget->m_labels.at( i ) );
                x += w;
                /// draw candidate
                p.setFont( m_candidateFont );
                p.setPen( m_candidateColor );
                w = p.fontMetrics().width( widget->m_candidates.at( i ).trimmed() );
                p.drawText( x, y, w, h, Qt::AlignCenter, widget->m_candidates.at( i ).trimmed() );
                y += h;
            }
        }
        else {
            for ( int i = 0; i < count; ++i ) {
                /// draw label
                p.setFont( m_labelFont );
                p.setPen( m_labelColor );
                w = p.fontMetrics().width( widget->m_labels.at( i ) );
                p.drawText( x, y, w, h, Qt::AlignCenter, widget->m_labels.at( i ) );
                x += w;
                /// draw candidate
                p.setFont( m_candidateFont );
                p.setPen( m_candidateColor );
                w = p.fontMetrics().width( widget->m_candidates.at( i ).trimmed() + ' ' );
                p.drawText( x, y, w, h, Qt::AlignCenter, widget->m_candidates.at( i ).trimmed() + ' ' );
                x += w;
            }
        }
    }
}

void ThemerSogou::drawStatusBar( StatusBar* widget )
{
    QPainter p( widget );

    if ( KIMToySettings::self()->backgroundColorizing() ) {
        QPainterPath path;
        path.addRegion( m_statusBarMask );
        p.fillPath( path, KIMToySettings::self()->statusBarColorize() );
    }

    p.drawPixmap( 0, 0, m_statusBarSkin );
}

void ThemerSogou::drawPropertyWidget( PropertyWidget* widget )
{
    QPainter p( widget );
    if ( !widget->m_iconName.isEmpty() )
        p.drawPixmap( widget->rect(), MainBarIcon( widget->m_iconName ) );
    else
        p.drawText( widget->rect(), Qt::AlignCenter, widget->m_name );
}
