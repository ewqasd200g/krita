/*
 *  Copyright (c) 2017 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "KoShapeFillWrapper.h"

#include <KoShape.h>
#include <QList>
#include <QBrush>
#include <KoColorBackground.h>
#include <KoGradientBackground.h>
#include <KoPatternBackground.h>
#include <KoShapeStroke.h>
#include <KoShapeBackgroundCommand.h>
#include <KoShapeStrokeCommand.h>
#include <KoStopGradient.h>

#include "kis_assert.h"
#include "kis_debug.h"
#include "kis_global.h"

#include <KoFlakeUtils.h>

struct ShapeBackgroundFetchPolicy
{
    typedef KoFlake::FillType Type;

    typedef QSharedPointer<KoShapeBackground> PointerType;
    static PointerType getBackground(KoShape *shape) {
        return shape->background();
    }
    static Type type(KoShape *shape) {
        QSharedPointer<KoShapeBackground> background = shape->background();
        QSharedPointer<KoColorBackground> colorBackground = qSharedPointerDynamicCast<KoColorBackground>(background);
        QSharedPointer<KoGradientBackground> gradientBackground = qSharedPointerDynamicCast<KoGradientBackground>(background);
        QSharedPointer<KoPatternBackground> patternBackground = qSharedPointerDynamicCast<KoPatternBackground>(background);

        return colorBackground ? Type::Solid :
               gradientBackground ? Type::Gradient :
               patternBackground ? Type::Pattern : Type::None;
    }

    static QColor color(KoShape *shape) {
        QSharedPointer<KoColorBackground> colorBackground = qSharedPointerDynamicCast<KoColorBackground>(shape->background());
        return colorBackground ? colorBackground->color() : QColor();
    }

    static const QGradient* gradient(KoShape *shape) {
        QSharedPointer<KoGradientBackground> gradientBackground = qSharedPointerDynamicCast<KoGradientBackground>(shape->background());
        return gradientBackground ? gradientBackground->gradient() : 0;
    }

    static QTransform gradientTransform(KoShape *shape) {
        QSharedPointer<KoGradientBackground> gradientBackground = qSharedPointerDynamicCast<KoGradientBackground>(shape->background());
        return gradientBackground ? gradientBackground->transform() : QTransform();
    }

    static bool compareTo(PointerType p1, PointerType p2) {
        return p1->compareTo(p2.data());
    }
};

struct ShapeStrokeFillFetchPolicy
{
    typedef KoFlake::FillType Type;

    typedef KoShapeStrokeModelSP PointerType;
    static PointerType getBackground(KoShape *shape) {
        return shape->stroke();
    }
    static Type type(KoShape *shape) {
        KoShapeStrokeSP stroke = qSharedPointerDynamicCast<KoShapeStroke>(shape->stroke());
        if (!stroke) return Type::None;

        // TODO: patterns are not supported yet!
        return stroke->lineBrush().gradient() ? Type::Gradient : Type::Solid;
    }

    static QColor color(KoShape *shape) {
        KoShapeStrokeSP stroke = qSharedPointerDynamicCast<KoShapeStroke>(shape->stroke());
        return stroke ? stroke->color() : QColor();
    }

    static const QGradient* gradient(KoShape *shape) {
        KoShapeStrokeSP stroke = qSharedPointerDynamicCast<KoShapeStroke>(shape->stroke());
        return stroke ? stroke->lineBrush().gradient() : 0;
    }

    static QTransform gradientTransform(KoShape *shape) {
        KoShapeStrokeSP stroke = qSharedPointerDynamicCast<KoShapeStroke>(shape->stroke());
        return stroke ? stroke->lineBrush().transform() : QTransform();
    }

    static bool compareTo(PointerType p1, PointerType p2) {
        return p1->compareFillTo(p2.data());
    }
};


template <class Policy>
bool compareBackgrounds(const QList<KoShape*> shapes)
{
    if (shapes.size() == 1) return true;

    typename Policy::PointerType bg =
        Policy::getBackground(shapes.first());

    Q_FOREACH (KoShape *shape, shapes) {
        if (
            !(
              (!bg && !Policy::getBackground(shape)) ||
              (bg && Policy::compareTo(bg, Policy::getBackground(shape)))
             )) {

            return false;
        }
    }

    return true;
}

/******************************************************************************/
/*             KoShapeFillWrapper::Private                                    */
/******************************************************************************/

struct KoShapeFillWrapper::Private
{
    QList<KoShape*> shapes;
    KoFlake::FillVariant fillVariant= KoFlake::Fill;

    QSharedPointer<KoShapeBackground> applyFillGradientStops(KoShape *shape, const QGradient *srcQGradient);
    void applyFillGradientStops(KoShapeStrokeSP shapeStroke, const QGradient *stopGradient);
};

QSharedPointer<KoShapeBackground> KoShapeFillWrapper::Private::applyFillGradientStops(KoShape *shape, const QGradient *stopGradient)
{
    QGradientStops stops = stopGradient->stops();

    if (!shape || !stops.count()) {
        return QSharedPointer<KoShapeBackground>();
    }

    KoGradientBackground *newGradient = 0;
    QSharedPointer<KoGradientBackground> oldGradient = qSharedPointerDynamicCast<KoGradientBackground>(shape->background());
    if (oldGradient) {
        // just copy the gradient and set the new stops
        QGradient *g = KoFlake::mergeGradient(oldGradient->gradient(), stopGradient);
        newGradient = new KoGradientBackground(g);
        newGradient->setTransform(oldGradient->transform());
    }
    else {
        // No gradient yet, so create a new one.
        QScopedPointer<QLinearGradient> fakeShapeGradient(new QLinearGradient(QPointF(0, 0), QPointF(1, 1)));
        fakeShapeGradient->setCoordinateMode(QGradient::ObjectBoundingMode);

        QGradient *g = KoFlake::mergeGradient(fakeShapeGradient.data(), stopGradient);
        newGradient = new KoGradientBackground(g);
    }
    return QSharedPointer<KoGradientBackground>(newGradient);
}

void KoShapeFillWrapper::Private::applyFillGradientStops(KoShapeStrokeSP shapeStroke, const QGradient *stopGradient)
{
    QGradientStops stops = stopGradient->stops();
    if (!stops.count()) return;

    QLinearGradient fakeShapeGradient(QPointF(0, 0), QPointF(1, 1));
    fakeShapeGradient.setCoordinateMode(QGradient::ObjectBoundingMode);
    QTransform gradientTransform;
    const QGradient *shapeGradient = 0;

    {
        QBrush brush = shapeStroke->lineBrush();
        gradientTransform = brush.transform();
        shapeGradient = brush.gradient() ? brush.gradient() : &fakeShapeGradient;
    }

    {
        QScopedPointer<QGradient> g(KoFlake::mergeGradient(shapeGradient, stopGradient));
        QBrush newBrush = *g;
        newBrush.setTransform(gradientTransform);
        shapeStroke->setLineBrush(newBrush);
    }
}

/******************************************************************************/
/*             KoShapeFillWrapper                                             */
/******************************************************************************/

KoShapeFillWrapper::KoShapeFillWrapper(KoShape *shape, KoFlake::FillVariant fillVariant)
    : m_d(new Private())
{
    KIS_SAFE_ASSERT_RECOVER_RETURN(shape);
    m_d->shapes << shape;
    m_d->fillVariant= fillVariant;
}


KoShapeFillWrapper::KoShapeFillWrapper(QList<KoShape*> shapes, KoFlake::FillVariant fillVariant)
    : m_d(new Private())
{
    KIS_SAFE_ASSERT_RECOVER_RETURN(!shapes.isEmpty());
    m_d->shapes = shapes;
    m_d->fillVariant= fillVariant;
}

KoShapeFillWrapper::~KoShapeFillWrapper()
{
}

bool KoShapeFillWrapper::isMixedFill() const
{
    if (m_d->shapes.isEmpty()) return false;

    return m_d->fillVariant == KoFlake::Fill ?
        !compareBackgrounds<ShapeBackgroundFetchPolicy>(m_d->shapes) :
        !compareBackgrounds<ShapeStrokeFillFetchPolicy>(m_d->shapes);
}

KoFlake::FillType KoShapeFillWrapper::type() const
{
    if (m_d->shapes.isEmpty() || isMixedFill()) return KoFlake::None;

    KoShape *shape = m_d->shapes.first();
    KIS_SAFE_ASSERT_RECOVER_RETURN_VALUE(shape, KoFlake::None);

    return m_d->fillVariant == KoFlake::Fill ?
        ShapeBackgroundFetchPolicy::type(shape) :
        ShapeStrokeFillFetchPolicy::type(shape);
}

QColor KoShapeFillWrapper::color() const
{
    // this check guarantees that the shapes list is not empty and
    // the fill is not mixed!
    if (type() != KoFlake::Solid) return QColor();

    KoShape *shape = m_d->shapes.first();
    KIS_SAFE_ASSERT_RECOVER_RETURN_VALUE(shape, QColor());

    return m_d->fillVariant == KoFlake::Fill ?
        ShapeBackgroundFetchPolicy::color(shape) :
        ShapeStrokeFillFetchPolicy::color(shape);
}

const QGradient* KoShapeFillWrapper::gradient() const
{
    // this check guarantees that the shapes list is not empty and
    // the fill is not mixed!
    if (type() != KoFlake::Gradient) return 0;

    KoShape *shape = m_d->shapes.first();
    KIS_SAFE_ASSERT_RECOVER_RETURN_VALUE(shape, 0);

    return m_d->fillVariant == KoFlake::Fill ?
        ShapeBackgroundFetchPolicy::gradient(shape) :
        ShapeStrokeFillFetchPolicy::gradient(shape);
}

QTransform KoShapeFillWrapper::gradientTransform() const
{
    // this check guarantees that the shapes list is not empty and
    // the fill is not mixed!
    if (type() != KoFlake::Gradient) return QTransform();

    KoShape *shape = m_d->shapes.first();
    KIS_SAFE_ASSERT_RECOVER_RETURN_VALUE(shape, QTransform());

    return m_d->fillVariant == KoFlake::Fill ?
        ShapeBackgroundFetchPolicy::gradientTransform(shape) :
                ShapeStrokeFillFetchPolicy::gradientTransform(shape);
}

KUndo2Command *KoShapeFillWrapper::setColor(const QColor &color)
{
    KUndo2Command *command = 0;

    if (m_d->fillVariant == KoFlake::Fill) {
         QSharedPointer<KoShapeBackground> bg;

        if (color.isValid()) {
            bg = toQShared(new KoColorBackground(color));
        }

        QSharedPointer<KoShapeBackground> fill(bg);
        command = new KoShapeBackgroundCommand(m_d->shapes, fill);
    } else {
        command = KoFlake::modifyShapesStrokes(m_d->shapes,
            [color] (KoShapeStrokeSP stroke) {
                stroke->setLineBrush(Qt::NoBrush);
                stroke->setColor(color.isValid() ? color : Qt::transparent);

            });
    }

    return command;
}

KUndo2Command *KoShapeFillWrapper::setGradient(const QGradient *gradient, const QTransform &transform)
{
    KUndo2Command *command = 0;

    if (m_d->fillVariant == KoFlake::Fill) {
        QList<QSharedPointer<KoShapeBackground>> newBackgrounds;

        foreach (KoShape *shape, m_d->shapes) {
            Q_UNUSED(shape);

            KoGradientBackground *newGradient = new KoGradientBackground(KoFlake::cloneGradient(gradient));
            newGradient->setTransform(transform);
            newBackgrounds << toQShared(newGradient);
        }

        command = new KoShapeBackgroundCommand(m_d->shapes, newBackgrounds);

    } else {
        command = KoFlake::modifyShapesStrokes(m_d->shapes,
            [gradient, transform] (KoShapeStrokeSP stroke) {
                QBrush newBrush = *gradient;
                newBrush.setTransform(transform);

                stroke->setLineBrush(newBrush);
                stroke->setColor(Qt::transparent);
            });
    }

    return command;
}

KUndo2Command* KoShapeFillWrapper::applyGradient(const QGradient *gradient)
{
    return setGradient(gradient, gradientTransform());
}

KUndo2Command* KoShapeFillWrapper::applyGradientStopsOnly(const QGradient *gradient)
{
    KUndo2Command *command = 0;

    if (m_d->fillVariant == KoFlake::Fill) {
        QList<QSharedPointer<KoShapeBackground>> newBackgrounds;

        foreach (KoShape *shape, m_d->shapes) {
            newBackgrounds <<  m_d->applyFillGradientStops(shape, gradient);
        }

        command = new KoShapeBackgroundCommand(m_d->shapes, newBackgrounds);

    } else {
        command = KoFlake::modifyShapesStrokes(m_d->shapes,
            [this, gradient] (KoShapeStrokeSP stroke) {
                m_d->applyFillGradientStops(stroke, gradient);
            });
    }

    return command;
}
