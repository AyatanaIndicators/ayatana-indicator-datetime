/*
 * Copyright 2024-2025 Robert Tari
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 3, as published
 * by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranties of
 * MERCHANTABILITY, SATISFACTORY QUALITY, or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *   Robert Tari <robert@tari.in>
 */

#include <gio/gio.h>
#include <datetime/engine-eds.h>
#include <datetime/myself.h>
#include <extendedstorage.h>
#include <QDebug>

namespace ayatana
{
    namespace indicator
    {
        namespace datetime
        {
            class EdsEngine::Impl
            {
            public:

                explicit Impl (const std::shared_ptr<Myself> &myself) : pMyself (myself)
                {
                    this->pMyself->emails ().changed ().connect ([this] (const std::set<std::string> &)
                    {
                        setDirtySoon ();
                    });

                    // mKCal::ExtendedStorageObserver does not work
                    const gchar* sHome = g_get_home_dir ();
                    gchar *sDatabase = g_strdup_printf ("%s/.local/share/system/privileged/Calendar/mkcal/db.changed", sHome);
                    GFile *pFile = g_file_new_for_path (sDatabase);
                    g_free (sDatabase);
                    GError *pError = NULL;
                    this->pMonitor = g_file_monitor_file (pFile, G_FILE_MONITOR_NONE, NULL, &pError);
                    g_object_unref (pFile);

                    if (!this->pMonitor)
                    {
                        g_warning ("Panic: Error creating file monitor: %s", pError->message);
                        g_error_free (pError);
                    }
                    else
                    {
                        g_signal_connect (this->pMonitor, "changed", G_CALLBACK (onDatabaseChanged), this);
                    }
                }

                ~Impl ()
                {
                    if (this->pMonitor)
                    {
                        g_object_unref (this->pMonitor);
                    }
                }

                core::Signal<>& changed ()
                {
                    return this->pChanged;
                }

                void addAlarm (KCalendarCore::Alarm::Ptr pAlarm, std::map<std::pair<DateTime, DateTime>, std::map<DateTime, Alarm>> *pAlarms, QDateTime *pTriggerTime, QTimeZone *pTimeZone, KCalendarCore::Incidence::Ptr pIncidence, bool bAlarm)
                {
                    /*
                    Loop through lAlarms to get information that we need to build the instance appointments and their alarms.

                    Outer map key is the instance component's start + end time. We build Appointment.begin and .end from that.

                    Inner map key is the alarm trigger, we build Alarm.time from that.

                    Inner map value is the Alarm.

                    We map the alarms based on their trigger time so that we can fold together multiple valarms that trigger for the same component at the same time. This is commonplace;
                    e.g. one valarm will have a display action and another will specify a sound to be played.
                    */
                    DateTime cBeginTime = datetimeFromQDateTime (pTriggerTime, pTimeZone);
                    QDateTime cQEndTime = {};
                    const bool bTime = pAlarm->hasTime ();

                    if (!bTime)
                    {
                        cQEndTime = *pTriggerTime;
                    }
                    else
                    {
                        cQEndTime = pAlarm->endTime ();
                    }

                    DateTime cEndTime = datetimeFromQDateTime (&cQEndTime, pTimeZone);
                    auto dInstanceTime = std::make_pair (cBeginTime, cEndTime);
                    DateTime cTriggerTime = datetimeFromQDateTime (pTriggerTime, pTimeZone);
                    auto &cAlarm = (*pAlarms)[dInstanceTime][cTriggerTime];
                    bool bTextEmpty = cAlarm.text.empty ();

                    if (bTextEmpty)
                    {
                        const KCalendarCore::Alarm::Type eType = pAlarm->type ();

                        if (eType == KCalendarCore::Alarm::Type::Display)
                        {
                            const QString sText = pAlarm->text ();
                            bool bEmpty = sText.isEmpty ();

                            if (!bEmpty)
                            {
                                cAlarm.text = sText.toStdString ();
                            }
                        }
                    }

                    bool bAudioEmpty = cAlarm.audio_url.empty ();

                    if (bAudioEmpty)
                    {
                        QString sUri = {};
                        const KCalendarCore::Alarm::Type eType = pAlarm->type ();

                        if (eType == KCalendarCore::Alarm::Type::Audio)
                        {
                            const QString sFile = pAlarm->audioFile ();
                            bool bEmpty = sFile.isEmpty ();

                            if (bEmpty)
                            {
                                cAlarm.audio_url = bAlarm ? "file://" ALARM_DEFAULT_SOUND : "file://" CALENDAR_DEFAULT_SOUND;
                            }
                            else
                            {
                                cAlarm.audio_url = sFile.toStdString ();
                            }
                        }
                    }

                    bool bTimeSet = cAlarm.time.is_set ();

                    if (!bTimeSet)
                    {
                        cAlarm.time = cTriggerTime;
                    }
                }

                void getAppointments (const QDateTime *pDateTimeBegin, const QDateTime *pDateTimeEnd, QTimeZone *pTimezone, std::function<void (const std::vector<Appointment>&)> pOnAppointments)
                {
                    qDebug () << "Getting all appointments from " << *pDateTimeBegin << " to " << *pDateTimeEnd;

                    const QDate cDateBegin = pDateTimeBegin->date ();
                    const QDate cDateEnd = pDateTimeEnd->date ();

                    // Load the incidences
                    QTimeZone cSystemTimeZone = QTimeZone::systemTimeZone ();
                    mKCal::ExtendedCalendar *pCalendar = new mKCal::ExtendedCalendar (cSystemTimeZone);
                    mKCal::ExtendedCalendar::Ptr pCalendarPtr = mKCal::ExtendedCalendar::Ptr (pCalendar);
                    mKCal::ExtendedStorage::Ptr pStoragePtr = mKCal::ExtendedCalendar::defaultStorage (pCalendarPtr);
                    pStoragePtr->open ();
                    pStoragePtr->load (cDateBegin, cDateEnd);

                    // Get the notebooks
                    mKCal::Notebook::List lNotebooks = pStoragePtr->notebooks ();
                    std::map<QString, mKCal::Notebook::Ptr> dNotebooks;

                    for (mKCal::Notebook::Ptr pNotebook : lNotebooks)
                    {
                        QString sUid = pNotebook->uid ();
                        dNotebooks[sUid] = pNotebook;
                    }

                    std::vector<Appointment> lAppointments;
                    KCalendarCore::Incidence::List lIncidences = pCalendarPtr->incidences ();

                    std::sort (lIncidences.begin (), lIncidences.end (), [](const KCalendarCore::Incidence::Ptr &pIncidence1, const KCalendarCore::Incidence::Ptr &pIncidence2)
                    {
                        return (pIncidence1->dtStart () < pIncidence2->dtStart ());
                    });

                    // Walk through the incidences to build the appointment list
                    for (KCalendarCore::Incidence::Ptr pIncidence : lIncidences)
                    {
                        QString sCalendar = pCalendarPtr->notebook (pIncidence);
                        bool bAlarm = dNotebooks[sCalendar]->name () == "Alarms";
                        const QString sUid = pIncidence->uid ();
                        bool bEmpty = sUid.isEmpty ();

                        if (bEmpty == false)
                        {
                            bool bIncidenceInteresting = isIncidenceInteresting (pIncidence);

                            if (bIncidenceInteresting)
                            {
                                const QString sColor = dNotebooks[sCalendar]->color ();
                                std::string sColorStd = sColor.toStdString ();
                                Appointment cAppointment = getAppointment (pIncidence, pTimezone, bAlarm, sColorStd);
                                Appointment *pAppointment = nullptr;

                                if (!bAlarm)
                                {
                                    // Walk the recurrences and add them
                                    const KCalendarCore::Recurrence *pRecurrence = pIncidence->recurrence ();
                                    const bool bRecurs = pRecurrence->recurs ();

                                    if (bRecurs)
                                    {
                                        QDateTime cRecurrenceStart = pRecurrence->getNextDateTime (*pDateTimeBegin);
                                        bool bRecurrenceValid = cRecurrenceStart.isValid ();

                                        while (bRecurrenceValid && cRecurrenceStart <= *pDateTimeEnd)
                                        {
                                            Appointment cRecurringAppointment = cAppointment;
                                            cRecurringAppointment.begin = datetimeFromQDateTime (&cRecurrenceStart, pTimezone);
                                            /*const int64_t nAppointmentEnd = cRecurringAppointment.end.to_unix ();
                                            QDateTime cRecurrenceEnd = QDateTime::fromSecsSinceEpoch (nAppointmentEnd);
                                            const qint64 nRecurrenceStart = cRecurrenceStart.toSecsSinceEpoch ();
                                            QDateTime cIncidenceStart = pIncidence->dtStart ();
                                            const qint64 nIncidenceStart = cIncidenceStart.toSecsSinceEpoch ();
                                            cRecurrenceEnd = cRecurrenceEnd.addSecs (nRecurrenceStart - nIncidenceStart);*/
                                            const QDateTime cRecurrenceEnd = pIncidence->endDateForStart (cRecurrenceStart);
                                            cRecurringAppointment.end = datetimeFromQDateTime (&cRecurrenceEnd, pTimezone);
                                            lAppointments.push_back (cRecurringAppointment);
                                            qDebug () << "Recurrence from " << cRecurringAppointment.begin.format ("%F %T %z").c_str () << " to " << cRecurringAppointment.end.format ("%F %T %z").c_str () << ":" << cRecurringAppointment.summary.c_str ();
                                            cRecurrenceStart = pRecurrence->getNextDateTime (cRecurrenceStart);
                                            bRecurrenceValid = cRecurrenceStart.isValid ();
                                        }
                                    }
                                    else
                                    {
                                        lAppointments.push_back (cAppointment);
                                        qDebug () << "Event from " << cAppointment.begin.format ("%F %T %z").c_str () << " to " << cAppointment.end.format ("%F %T %z").c_str () << ":" << cAppointment.summary.c_str ();

                                    }

                                    pAppointment = &lAppointments.back ();
                                }

                                // Generate alarms
                                KCalendarCore::Alarm::List lAlarms = pIncidence->alarms ();
                                std::map<std::pair<DateTime, DateTime>, std::map<DateTime, Alarm>> dAlarms;

                                // Walk the alarms and add them
                                for (KCalendarCore::Alarm::Ptr pAlarm : lAlarms)
                                {
                                    // we only care about AUDIO or DISPLAY alarms, other kind of alarm will not generate a notification
                                    bool bAlarmInteresting = isAlarmInteresting (pAlarm);

                                    if (bAlarmInteresting)
                                    {
                                        const int nRepeat = pAlarm->repeatCount ();
                                        const KCalendarCore::Duration cSnoozeTime = pAlarm->snoozeTime ();
                                        const KCalendarCore::Duration cStartOffset = pAlarm->startOffset ();
                                        const int nStartOffset = cStartOffset.asSeconds ();

                                        if (nRepeat && cSnoozeTime)
                                        {
                                            const int nSnoozeTime = cSnoozeTime.asSeconds ();

                                            for (int nIter = 0; nIter < nRepeat + 1; nIter++)
                                            {
                                                QDateTime cStartDateTime = pIncidence->dtStart ();
                                                QDateTime cTriggerTime = cStartDateTime.addSecs (nStartOffset + (nIter * nSnoozeTime));
                                                const Qt::TimeSpec cTimeSpec = cTriggerTime.timeSpec ();

                                                if (cTimeSpec != Qt::LocalTime)
                                                {
                                                    cTriggerTime = cTriggerTime.toTimeZone (*pTimezone);
                                                }

                                                this->addAlarm (pAlarm, &dAlarms, &cTriggerTime, pTimezone, pIncidence, bAlarm);
                                            }
                                        }
                                        else
                                        {
                                            QDateTime cTriggerTime = (*pDateTimeBegin).addSecs (nStartOffset);
                                            cTriggerTime = pAlarm->nextRepetition (cTriggerTime);
                                            bool bValid = cTriggerTime.isValid ();

                                            while (bValid && cTriggerTime <= *pDateTimeEnd)
                                            {
                                                const Qt::TimeSpec cTimeSpec = cTriggerTime.timeSpec ();

                                                if (cTimeSpec != Qt::LocalTime)
                                                {
                                                    cTriggerTime = cTriggerTime.toTimeZone (*pTimezone);
                                                }

                                                this->addAlarm (pAlarm, &dAlarms, &cTriggerTime, pTimezone, pIncidence, bAlarm);
                                                cTriggerTime = pAlarm->nextRepetition (cTriggerTime);
                                                bValid = cTriggerTime.isValid ();
                                            }
                                        }
                                    }
                                }

                                for (auto &cAlarm : dAlarms)
                                {
                                    if (bAlarm)
                                    {
                                        pAppointment = new Appointment (cAppointment);
                                        pAppointment->begin = cAlarm.first.first;
                                        pAppointment->end = cAlarm.first.second;
                                    }

                                    int nAlarms = cAlarm.second.size ();
                                    pAppointment->alarms.reserve (nAlarms);

                                    for (auto &cAlarmSecond : cAlarm.second)
                                    {
                                        bool bText = cAlarmSecond.second.has_text ();
                                        bool bSound = cAlarmSecond.second.has_sound ();

                                        if (bText || bSound)
                                        {
                                            pAppointment->alarms.push_back (cAlarmSecond.second);
                                            qDebug () << "Alarm at " << cAlarmSecond.second.time.format ("%F %T %z").c_str ();
                                        }
                                    }

                                    if (bAlarm)
                                    {
                                        lAppointments.push_back (*pAppointment);
                                        qDebug () << "Alarm from " << (*pAppointment).begin.format ("%F %T %z").c_str () << " to " << (*pAppointment).end.format ("%F %T %z").c_str () << ":" << (*pAppointment).summary.c_str ();
                                        delete pAppointment;
                                    }
                                }
                            }
                        }
                        else
                        {
                            qWarning () << "Panic: incidence has no UID" << pIncidence;
                        }
                    }

                    // Give the caller the sorted finished product
                    auto &pAppointments = lAppointments;
                    std::sort (pAppointments.begin (), pAppointments.end (), [](const Appointment &pAppointment1, const Appointment &pAppointment2){return pAppointment1.begin < pAppointment2.begin;});
                    pOnAppointments (pAppointments);

                    qDebug () << "Returning appointments: " << lAppointments.size ();

                    pStoragePtr.clear ();
                }

            private:

                static void onDatabaseChanged (GFileMonitor *pMonitor, GFile *pFile, GFile *OtherFile, GFileMonitorEvent eEvent, gpointer pData)
                {
                    auto pSelf = static_cast<Impl*> (pData);

                    if (eEvent == G_FILE_MONITOR_EVENT_CHANGED)
                    {
                        pSelf->setDirtySoon ();
                    }
                }

                void setDirtyNow ()
                {
                    this->pChanged ();
                }

                static gboolean setDirtyNowStatic (gpointer pSelfPtr)
                {
                    auto pSelf = static_cast<Impl*> (pSelfPtr);
                    pSelf->nRebuildTag = 0;
                    pSelf->nRebuildDeadline = 0;
                    pSelf->setDirtyNow ();

                    return G_SOURCE_REMOVE;
                }

                void setDirtySoon ()
                {
                    static constexpr int MIN_BATCH_SEC = 1;
                    static constexpr int MAX_BATCH_SEC = 60;
                    static_assert (MIN_BATCH_SEC <= MAX_BATCH_SEC, "bad boundaries");
                    const auto nNow = time (nullptr);

                    // First pass
                    if (this->nRebuildDeadline == 0)
                    {
                        this->nRebuildDeadline = nNow + MAX_BATCH_SEC;
                        this->nRebuildTag = g_timeout_add_seconds (MIN_BATCH_SEC, setDirtyNowStatic, this);
                    }
                    else if (nNow < this->nRebuildDeadline)
                    {
                        g_source_remove (this->nRebuildTag);
                        this->nRebuildTag = g_timeout_add_seconds (MIN_BATCH_SEC, setDirtyNowStatic, this);
                    }
                }

                static bool isAlarmInteresting (KCalendarCore::Alarm::Ptr pAlarm)
                {
                    const KCalendarCore::Alarm::Type eType = pAlarm->type ();

                    if ((eType == KCalendarCore::Alarm::Type::Audio) || (eType == KCalendarCore::Alarm::Type::Display))
                    {
                        // We don't want disabled alarms
                        const bool bEnabled = pAlarm->enabled ();

                        return bEnabled;
                    }

                    return false;
                }

                static DateTime datetimeFromQDateTime (const QDateTime *pDateTime, QTimeZone *pTimeZone)
                {
                    DateTime cDateTimeOut = {};
                    bool bValid = pDateTime->isValid ();

                    if (!bValid)
                    {
                        return cDateTimeOut;
                    }

                    const QByteArray sId = pTimeZone->id ();
                    const char *sIdData = sId.constData ();
                    GTimeZone *pGTimeZone = g_time_zone_new_identifier (sIdData);
                    const QDate cDate = pDateTime->date ();
                    const QTime cTime = pDateTime->time ();
                    const int nYear = cDate.year ();
                    const int nMonth = cDate.month ();
                    const int nDay = cDate.day ();
                    const int nHour = cTime.hour ();
                    const int nMinute = cTime.minute ();
                    const int nSecond = cTime.second ();
                    cDateTimeOut = DateTime (pGTimeZone, nYear, nMonth, nDay, nHour, nMinute, nSecond);
                    g_time_zone_unref (pGTimeZone);

                    return cDateTimeOut;
                }

                bool isIncidenceInteresting (KCalendarCore::Incidence::Ptr pIncidence)
                {
                    // We only want calendar events and todos
                    const KCalendarCore::IncidenceBase::IncidenceType eType = pIncidence->type ();

                    if ((eType != KCalendarCore::IncidenceBase::IncidenceType::TypeEvent) && (eType != KCalendarCore::IncidenceBase::IncidenceType::TypeTodo))
                    {
                        return false;
                    }

                    // We're not interested in completed or cancelled incidences
                    const KCalendarCore::Incidence::Status eIncidenceStatus = pIncidence->status ();

                    if ((eIncidenceStatus == KCalendarCore::Incidence::Status::StatusCompleted) || (eIncidenceStatus == KCalendarCore::Incidence::Status::StatusCanceled))
                    {
                        return false;
                    }

                    // We don't want not attending alarms
                    const KCalendarCore::Attendee::List lAttendees = pIncidence->attendees ();

                    for (KCalendarCore::Attendee cAttendee : lAttendees)
                    {
                        const QString sEmail = cAttendee.email ();
                        const std::string sEmailStd = sEmail.toStdString ();
                        bool bMyEmail = this->pMyself->isMyEmail (sEmailStd);

                        // Check if the user is part of the attendee list
                        if (bMyEmail)
                        {
                            // Check the status
                            const KCalendarCore::Attendee::PartStat eAttendeeStatus = cAttendee.status ();
                            bool bDeclined = (eAttendeeStatus == KCalendarCore::Attendee::PartStat::Declined);

                            return !bDeclined;
                        }
                    }

                    return true;
                }

                static Appointment getAppointment (KCalendarCore::Incidence::Ptr pIncidence, QTimeZone *pTimeZone, bool bAlarm, std::string sColor)
                {
                    Appointment cAppointment;

                    // Get Appointment.uid
                    const QString sUid = pIncidence->uid ();
                    cAppointment.uid = sUid.toStdString ();

                    // Get Appointment.summary
                    const QString sSummary = pIncidence->summary ();
                    cAppointment.summary = sSummary.toStdString ();

                    // Get Appointment.begin
                    QDateTime cBegin = pIncidence->dtStart ();
                    const Qt::TimeSpec cTimeSpecBegin = cBegin.timeSpec ();

                    if (cTimeSpecBegin != Qt::LocalTime)
                    {
                        cBegin = cBegin.toTimeZone (*pTimeZone);
                    }

                    cAppointment.begin = datetimeFromQDateTime (&cBegin, pTimeZone);

                    // Get Appointment.end
                    QDateTime cEnd = {};
                    const KCalendarCore::IncidenceBase::IncidenceType eType = pIncidence->type ();

                    if (eType == KCalendarCore::IncidenceBase::IncidenceType::TypeEvent)
                    {
                        KCalendarCore::Event::Ptr pEvent = qSharedPointerCast<KCalendarCore::Event> (pIncidence);
                        cEnd = pEvent->dtEnd ();
                        const Qt::TimeSpec cTimeSpecEnd = cEnd.timeSpec ();

                        if (cTimeSpecEnd != Qt::LocalTime)
                        {
                            cEnd = cEnd.toTimeZone (*pTimeZone);
                        }

                        // Check for all day event
                        bool bHasEndDate = pEvent->hasEndDate ();
                        bool bAllDay = pIncidence->allDay ();

                        if (!bHasEndDate && bAllDay)
                        {
                            cEnd = cBegin.addDays (1);
                        }
                    }

                    const bool bValid = cEnd.isValid ();

                    if (bValid)
                    {
                        cAppointment.end = datetimeFromQDateTime (&cEnd, pTimeZone);
                    }
                    else
                    {
                        cAppointment.end = cAppointment.begin;
                    }

                    // Get Appointment.type
                    if (bAlarm)
                    {
                        cAppointment.type = Appointment::ALARM;
                    }
                    else
                    {
                        cAppointment.type = Appointment::EVENT;
                    }

                    // Get Appointment.color
                    cAppointment.color = sColor;

                    return cAppointment;
                }

                core::Signal<> pChanged;
                guint nRebuildTag {};
                time_t nRebuildDeadline {};
                std::shared_ptr<Myself> pMyself;
                GFileMonitor *pMonitor;
            };

            EdsEngine::EdsEngine (const std::shared_ptr<Myself> &myself): p (new Impl (myself))
            {
            }

            EdsEngine::~EdsEngine () = default;

            core::Signal<>& EdsEngine::changed ()
            {
                return this->p->changed ();
            }

            void EdsEngine::get_appointments (const DateTime &pDateTimeBegin, const DateTime &pDateTimeEnd, const Timezone &pTimezone, std::function<void (const std::vector<Appointment>&)> pFunc)
            {
                qint64 nDateTimeBegin = pDateTimeBegin.to_unix ();
                QDateTime cDateTimeBegin = QDateTime::fromSecsSinceEpoch (nDateTimeBegin, Qt::UTC);
                qint64 nDateTimeEnd = pDateTimeEnd.to_unix ();
                QDateTime cDateTimeEnd = QDateTime::fromSecsSinceEpoch (nDateTimeEnd, Qt::UTC);
                QTimeZone cTimeZone = QTimeZone ();
                std::string sTimeZone = pTimezone.timezone.get ();
                const char *sTimeZoneData = sTimeZone.c_str ();
                QTimeZone cTimezone = QTimeZone (sTimeZoneData);
                this->p->getAppointments (&cDateTimeBegin, &cDateTimeEnd, &cTimezone, pFunc);
            }

            void EdsEngine::disable_alarm (const Appointment &pAppointment)
            {
                bool bAlarm = pAppointment.is_alarm ();

                if (bAlarm)
                {
                    QTimeZone cSystemTimeZone = QTimeZone::systemTimeZone ();
                    mKCal::ExtendedCalendar *pCalendar = new mKCal::ExtendedCalendar (cSystemTimeZone);
                    mKCal::ExtendedCalendar::Ptr pCalendarPtr = mKCal::ExtendedCalendar::Ptr (pCalendar);
                    mKCal::ExtendedStorage::Ptr pStoragePtr = mKCal::ExtendedCalendar::defaultStorage (pCalendarPtr);
                    pStoragePtr->open ();
                    const char *sUid = pAppointment.uid.c_str ();
                    pStoragePtr->load (sUid);
                    KCalendarCore::Incidence::List lIncidences = pCalendarPtr->incidences ();
                    bool bChanged = false;

                    for (KCalendarCore::Incidence::Ptr pIncidence : lIncidences)
                    {
                        KCalendarCore::Alarm::List lAlarms = pIncidence->alarms ();

                        for (KCalendarCore::Alarm::Ptr pAlarm : lAlarms)
                        {
                            const bool bEnabled = pAlarm->enabled ();

                            if (bEnabled)
                            {
                                pAlarm->setEnabled (false);
                                bChanged = true;
                            }
                        }
                    }

                    if (bChanged)
                    {
                        pStoragePtr->save ();
                    }

                    pStoragePtr.clear ();
                }
            }
        }
    }
}
