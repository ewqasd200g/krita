/*
 *  Copyright (c) 2013 Dmitry Kazakov <dimula73@gmail.com>
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

#ifndef __KIS_SIGNAL_COMPRESSOR_H
#define __KIS_SIGNAL_COMPRESSOR_H

#include <QTimer>
#include "kritaimage_export.h"

class QTimer;

/**
 * Sets a timer to delay or throttle activation of a Qt slot. One example of
 * where this is used is to limit the amount of expensive redraw activity on the
 * canvas.
 *
 * There are three behaviors to choose from.
 *
 * POSTPONE resets the timer after each call. Therefore if the calls are made
 * quickly enough, the timer will never be activated.
 *
 * FIRST_ACTIVE emits the timeout() event immediately and sets a timer of
 * duration \p delay. If the compressor is triggered during this time, it will
 * fire another signal at the end of the delay period. Further events are
 * ignored until the timer elapses. Think of it as a queue with size 1, and
 * where the leading element is popped every \p delay ms.
 *
 * FIRST_INACTIVE emits the timeout() event at the end of a timer of duration \p
 * delay ms. The compressor becomes inactive and all events are ignored until
 * the timer has elapsed.
 *
 */


class KRITAIMAGE_EXPORT KisSignalCompressor : public QObject
{
    Q_OBJECT

public:
    enum Mode {
        POSTPONE, /* Calling start() resets the timer to \p delay ms */
        FIRST_ACTIVE, /* Emit timeout() signal immediately. Throttle further timeout() to rate of one per \p delay ms */
        FIRST_INACTIVE, /* Set a timer \p delay ms, emit timeout() when it elapses. Ignore all events meanwhile. */
        UNDEFINED /* KisSignalCompressor is created without an explicit mode */
    };

public:
    KisSignalCompressor();
    KisSignalCompressor(int delay, Mode mode, QObject *parent = 0);
    bool isActive() const;
    void setMode(Mode mode);
    void setDelay(int delay);

public Q_SLOTS:
    void start();
    void stop();

private Q_SLOTS:
    void slotTimerExpired();

Q_SIGNALS:
    void timeout();

private:
    QTimer m_timer;
    Mode m_mode;
    bool m_gotSignals;
};

#endif /* __KIS_SIGNAL_COMPRESSOR_H */
