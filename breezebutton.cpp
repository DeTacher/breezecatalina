/*
 * SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>
 * SPDX-FileCopyrightText: 2014 Hugo Pereira Da Costa <hugo.pereira@free.fr>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */
#include "breezebutton.h"

#include <KColorUtils>
#include <KDecoration2/DecoratedClient>
#include <KIconLoader>

#include <QPainter>
#include <QPainterPath>
#include <QVariantAnimation>

namespace Breeze
{
using KDecoration2::ColorGroup;
using KDecoration2::ColorRole;
using KDecoration2::DecorationButtonType;

//__________________________________________________________________
Button::Button(DecorationButtonType type, Decoration *decoration, QObject *parent)
    : DecorationButton(type, decoration, parent)
    , m_animation(new QVariantAnimation(this))
{
    // setup animation
    // It is important start and end value are of the same type, hence 0.0 and not just 0
    m_animation->setStartValue(0.0);
    m_animation->setEndValue(1.0);
    m_animation->setEasingCurve(QEasingCurve::InOutQuad);
    connect(m_animation, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
        setOpacity(value.toReal());
    });

    // setup default geometry
    const int height = decoration->buttonHeight();
    setGeometry(QRect(0, 0, height, height));
    setIconSize(QSize(height, height));

    // connections
    connect(decoration->client().data(), SIGNAL(iconChanged(QIcon)), this, SLOT(update()));
    connect(decoration->settings().data(), &KDecoration2::DecorationSettings::reconfigured, this, &Button::reconfigure);
    connect(this, &KDecoration2::DecorationButton::hoveredChanged, this, &Button::updateAnimationState);

    reconfigure();
}

//__________________________________________________________________
Button::Button(QObject *parent, const QVariantList &args)
    : Button(args.at(0).value<DecorationButtonType>(), args.at(1).value<Decoration *>(), parent)
{
    m_flag = FlagStandalone;
    //! icon size must return to !valid because it was altered from the default constructor,
    //! in Standalone mode the button is not using the decoration metrics but its geometry
    m_iconSize = QSize(-1, -1);
}

//__________________________________________________________________
Button *Button::create(DecorationButtonType type, KDecoration2::Decoration *decoration, QObject *parent)
{
    if (auto d = qobject_cast<Decoration *>(decoration)) {
        Button *b = new Button(type, d, parent);
        switch (type) {
        case DecorationButtonType::Close:
            b->setVisible(d->client().data()->isCloseable());
            QObject::connect(d->client().data(), &KDecoration2::DecoratedClient::closeableChanged, b, &Breeze::Button::setVisible);
            break;

        case DecorationButtonType::Maximize:
            b->setVisible(d->client().data()->isMaximizeable());
            QObject::connect(d->client().data(), &KDecoration2::DecoratedClient::maximizeableChanged, b, &Breeze::Button::setVisible);
            break;

        case DecorationButtonType::Minimize:
            b->setVisible(d->client().data()->isMinimizeable());
            QObject::connect(d->client().data(), &KDecoration2::DecoratedClient::minimizeableChanged, b, &Breeze::Button::setVisible);
            break;

        case DecorationButtonType::ContextHelp:
            b->setVisible(d->client().data()->providesContextHelp());
            QObject::connect(d->client().data(), &KDecoration2::DecoratedClient::providesContextHelpChanged, b, &Breeze::Button::setVisible);
            break;

        case DecorationButtonType::Shade:
            b->setVisible(d->client().data()->isShadeable());
            QObject::connect(d->client().data(), &KDecoration2::DecoratedClient::shadeableChanged, b, &Breeze::Button::setVisible);
            break;

        case DecorationButtonType::Menu:
            QObject::connect(d->client().data(), &KDecoration2::DecoratedClient::iconChanged, b, [b]() {
                b->update();
            });
            break;

        default:
            break;
        }

        return b;
    }

    return nullptr;
}

//__________________________________________________________________
void Button::paint(QPainter *painter, const QRect &repaintRegion)
{
    Q_UNUSED(repaintRegion)

    if (!decoration())
        return;

    painter->save();

    // translate from offset
    if (m_flag == FlagFirstInList)
        painter->translate(m_offset);
    else
        painter->translate(0, m_offset.y());

    if (!m_iconSize.isValid())
        m_iconSize = geometry().size().toSize();

    // menu button
    if (type() == DecorationButtonType::Menu) {
        const QRectF iconRect(geometry().topLeft(), m_iconSize);
        if (auto deco = qobject_cast<Decoration *>(decoration())) {
            const QPalette activePalette = KIconLoader::global()->customPalette();
            QPalette palette = decoration()->client().data()->palette();
            palette.setColor(QPalette::Foreground, deco->fontColor());
            KIconLoader::global()->setCustomPalette(palette);
            decoration()->client().data()->icon().paint(painter, iconRect.toRect());
            if (activePalette == QPalette()) {
                KIconLoader::global()->resetPalette();
            } else {
                KIconLoader::global()->setCustomPalette(palette);
            }
        } else {
            decoration()->client().data()->icon().paint(painter, iconRect.toRect());
        }

    } else {
        drawIcon(painter);
    }

    painter->restore();
}

//__________________________________________________________________
void Button::drawIcon(QPainter *painter) const
{
    painter->setRenderHints(QPainter::Antialiasing);

    /*
    scale painter so that its window matches QRect( -1, -1, 20, 20 )
    this makes all further rendering and scaling simpler
    all further rendering is preformed inside QRect( 0, 0, 18, 18 )
    */
    painter->translate(geometry().topLeft());

    const qreal width(m_iconSize.width());
    painter->scale(width / 20, width / 20);
    painter->translate(1, 1);

    // render background
    const QColor backgroundColor(this->backgroundColor());
    if (backgroundColor.isValid()) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(backgroundColor);
        if (type() == DecorationButtonType::Maximize || type() == DecorationButtonType::Minimize || type() == DecorationButtonType::Close) {
            painter->drawEllipse(QRectF(2, 2, 14, 14));
        } else {
            painter->drawEllipse(QRectF(0, 0, 18, 18));
        }
    }

    // render mark
    const QColor foregroundColor(this->foregroundColor());
    if (foregroundColor.isValid()) {
        // setup painter
        QPen pen(foregroundColor);
        pen.setCapStyle(Qt::RoundCap);
        pen.setJoinStyle(Qt::MiterJoin);
        pen.setWidthF(PenWidth::Symbol * qMax((qreal)1.25, 20 / width));

        painter->setPen(pen);
        painter->setBrush(Qt::NoBrush);

        switch (type()) {
        case DecorationButtonType::Close: {
            if (isHovered()) {
                painter->drawLine(QPointF(6, 6), QPointF(12, 12));
                painter->drawLine(QPointF(12, 6), QPointF(6, 12));
            }
            break;
        }
        case DecorationButtonType::Maximize: {
            if (isHovered()) {
                if (isChecked()) {
                    {
                        QPainterPath path;
                        path.moveTo(5, 9);
                        path.lineTo(9, 9);
                        path.lineTo(9, 13);
                        path.lineTo(5, 9);

                        painter->setPen(Qt::NoPen);
                        painter->fillPath(path, foregroundColor);
                    }

                    {
                        QPainterPath path;
                        path.moveTo(9, 5);
                        path.lineTo(9, 9);
                        path.lineTo(13, 9);
                        path.lineTo(9, 5);

                        painter->setPen(Qt::NoPen);
                        painter->fillPath(path, foregroundColor);
                    }
                } else {
                    {
                        QPainterPath path;
                        path.moveTo(6, 8);
                        path.lineTo(6, 12);
                        path.lineTo(10, 12);
                        path.lineTo(6, 8);

                        painter->setPen(Qt::NoPen);
                        painter->fillPath(path, foregroundColor);
                    }

                    {
                        QPainterPath path;
                        path.moveTo(8, 6);
                        path.lineTo(12, 6);
                        path.lineTo(12, 10);
                        path.lineTo(8, 6);

                        painter->setPen(Qt::NoPen);
                        painter->fillPath(path, foregroundColor);
                    }
                }
            }
            break;
        }

        case DecorationButtonType::Minimize: {
            if (isHovered()) {
                painter->drawLine(QPointF(6, 9), QPointF(12, 9));
            }
            break;
        }

        case DecorationButtonType::OnAllDesktops: {
            painter->setPen(Qt::NoPen);
            painter->setBrush(foregroundColor);

            if (isChecked()) {
                // outer ring
                painter->drawEllipse(QRectF(3, 3, 12, 12));

                // center dot
                QColor backgroundColor(this->backgroundColor());
                auto d = qobject_cast<Decoration *>(decoration());
                if (!backgroundColor.isValid() && d)
                    backgroundColor = d->titleBarColor();

                if (backgroundColor.isValid()) {
                    painter->setBrush(backgroundColor);
                    painter->drawEllipse(QRectF(8, 8, 2, 2));
                }

            } else {
                painter->drawPolygon(QVector<QPointF> {QPointF(6.5, 8.5), QPointF(12, 3), QPointF(15, 6), QPointF(9.5, 11.5)});

                painter->setPen(pen);
                painter->drawLine(QPointF(5.5, 7.5), QPointF(10.5, 12.5));
                painter->drawLine(QPointF(12, 6), QPointF(4.5, 13.5));
            }
            break;
        }

        case DecorationButtonType::Shade: {
            if (isChecked()) {
                painter->drawLine(QPointF(4, 5.5), QPointF(14, 5.5));
                painter->drawPolyline(QVector<QPointF> {QPointF(4, 8), QPointF(9, 13), QPointF(14, 8)});

            } else {
                painter->drawLine(QPointF(4, 5.5), QPointF(14, 5.5));
                painter->drawPolyline(QVector<QPointF> {QPointF(4, 13), QPointF(9, 8), QPointF(14, 13)});
            }

            break;
        }

        case DecorationButtonType::KeepBelow: {
            painter->drawPolyline(QVector<QPointF> {QPointF(4, 5), QPointF(9, 10), QPointF(14, 5)});

            painter->drawPolyline(QVector<QPointF> {QPointF(4, 9), QPointF(9, 14), QPointF(14, 9)});
            break;
        }

        case DecorationButtonType::KeepAbove: {
            painter->drawPolyline(QVector<QPointF> {QPointF(4, 9), QPointF(9, 4), QPointF(14, 9)});

            painter->drawPolyline(QVector<QPointF> {QPointF(4, 13), QPointF(9, 8), QPointF(14, 13)});
            break;
        }

        case DecorationButtonType::ApplicationMenu: {
            painter->drawRect(QRectF(3.5, 4.5, 11, 1));
            painter->drawRect(QRectF(3.5, 8.5, 11, 1));
            painter->drawRect(QRectF(3.5, 12.5, 11, 1));
            break;
        }

        case DecorationButtonType::ContextHelp: {
            QPainterPath path;
            path.moveTo(5, 6);
            path.arcTo(QRectF(5, 3.5, 8, 5), 180, -180);
            path.cubicTo(QPointF(12.5, 9.5), QPointF(9, 7.5), QPointF(9, 11.5));
            painter->drawPath(path);

            painter->drawRect(QRectF(9, 15, 0.5, 0.5));

            break;
        }

        default:
            break;
        }
    }
}

//__________________________________________________________________
QColor Button::foregroundColor() const
{
    auto d = qobject_cast<Decoration *>(decoration());
    if (!d) {
        return QColor();

    } else if (type() == DecorationButtonType::Close || type() == DecorationButtonType::Minimize || type() == DecorationButtonType::Maximize) {
        return backgroundColor().darker(175);

    } else if (isPressed()) {
        return d->titleBarColor();

    } else if ((type() == DecorationButtonType::KeepBelow || type() == DecorationButtonType::KeepAbove || type() == DecorationButtonType::Shade) && isChecked()) {
        return d->titleBarColor();

    } else if (m_animation->state() == QAbstractAnimation::Running) {
        return KColorUtils::mix(d->fontColor(), d->titleBarColor(), m_opacity);

    } else if (isHovered()) {
        return d->titleBarColor();

    } else {
        return d->fontColor();
    }
}

//__________________________________________________________________
QColor Button::backgroundColor() const
{
    auto d = qobject_cast<Decoration *>(decoration());
    if (!d) {
        return QColor();
    }

    auto c = d->client().data();
    QColor closeColor(237, 101, 90);
    QColor maximizeColor(115, 190, 71);
    QColor minimizeColor(224, 192, 76);

    if (isPressed()) {
        if (type() == DecorationButtonType::Close)
            return d->fontColor();
        else if (type() == DecorationButtonType::Maximize)
            return d->fontColor();
        else if (type() == DecorationButtonType::Minimize)
            return d->fontColor();
        else
            return KColorUtils::mix(d->titleBarColor(), d->fontColor(), 0.3);

    } else if ((type() == DecorationButtonType::KeepBelow || type() == DecorationButtonType::KeepAbove || type() == DecorationButtonType::Shade) && isChecked()) {
        return d->fontColor();

    } else if (m_animation->state() == QAbstractAnimation::Running) {
        if (type() == DecorationButtonType::Close) {
            return d->fontColor();

        } else if (type() == DecorationButtonType::Maximize) {
            return d->fontColor();

        } else if (type() == DecorationButtonType::Minimize) {
            return d->fontColor();
        } else {
            return d->fontColor();
        }

    } else if (isHovered()) {
        if (type() == DecorationButtonType::Close)
            return d->fontColor();
        else if (type() == DecorationButtonType::Maximize)
            return d->fontColor();
        else if (type() == DecorationButtonType::Minimize)
            return d->fontColor();
        else
            return d->fontColor();

    } else if (type() == DecorationButtonType::Close) {
        return d->fontColor();

    } else if (type() == DecorationButtonType::Maximize) {
        return d->fontColor();

    } else if (type() == DecorationButtonType::Minimize) {
        return d->fontColor();

    } else {
        return d->fontColor();
    }
}

//________________________________________________________________
void Button::reconfigure()
{
    // animation
    auto d = qobject_cast<Decoration *>(decoration());
    if (d)
        m_animation->setDuration(d->animationsDuration());
}

//__________________________________________________________________
void Button::updateAnimationState(bool hovered)
{
    auto d = qobject_cast<Decoration *>(decoration());
    if (!(d && d->animationsDuration() > 0))
        return;

    m_animation->setDirection(hovered ? QAbstractAnimation::Forward : QAbstractAnimation::Backward);
    if (m_animation->state() != QAbstractAnimation::Running)
        m_animation->start();
}

} // namespace
