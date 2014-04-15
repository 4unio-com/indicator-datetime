/*
 * Copyright 2014 Canonical Ltd.
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
 *   Charles Kerr <charles.kerr@canonical.com>
 */

#include <datetime/engine-eds.h>

extern "C" {
    #include <libical/ical.h>
    #include <libical/icaltime.h>
    #include <libecal/libecal.h>
    #include <libedataserver/libedataserver.h>
}

#include <algorithm> // std::sort()
#include <ctime> // time()
#include <map>
#include <set>

namespace unity {
namespace indicator {
namespace datetime {

/****
*****
****/

class EdsSource
{
public:

    EdsSource(ESource* source, ECalClientSourceType source_type, core::Property<std::pair<DateTime,DateTime>>& range):
        m_source{E_SOURCE(g_object_ref(G_OBJECT(source)))},
        m_source_type{source_type},
        m_range(range)
    {
        e_cal_client_connect(source,
                             source_type,
                             m_cancellable,
                             on_client_connected,
                             this);

        auto extension = e_source_get_extension(source, E_SOURCE_EXTENSION_CALENDAR);
        const auto color = e_source_selectable_get_color(E_SOURCE_SELECTABLE(extension));
        if (color != nullptr)
            m_color = color;

        range.changed().connect([this](const std::pair<DateTime,DateTime>&){rebuild_view();});
    }

    ~EdsSource()
    {
        g_cancellable_cancel(m_cancellable);

        clear_view();

        g_object_unref(m_cancellable);

        g_clear_object(&m_client);
        g_clear_object(&m_source);
    }

    static bool get_preferred_type (ESource* source, ECalClientSourceType& setme)
    {
        bool client_wanted = false;

        if (e_source_has_extension(source, E_SOURCE_EXTENSION_CALENDAR))
        {
            setme = E_CAL_CLIENT_SOURCE_TYPE_EVENTS;
            client_wanted = true;
        }
        else if (e_source_has_extension(source, E_SOURCE_EXTENSION_TASK_LIST))
        {
            setme = E_CAL_CLIENT_SOURCE_TYPE_TASKS;
            client_wanted = true;
        }

        return client_wanted;
    }

    core::Property<std::vector<Appointment>>& appointments()
    {
        return m_appointments;
    }

private:

    static void on_client_connected(GObject* /*source*/, GAsyncResult * res, gpointer gself)
    {
        GError * error = nullptr;
        EClient * client = e_cal_client_connect_finish(res, &error);
        if (error)
        {
            if (!g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
                g_warning("indicator-datetime cannot connect to EDS source: %s", error->message);

            g_error_free(error);
        }
        else
        {
            g_debug("got a client for %s", e_cal_client_get_local_attachment_store(E_CAL_CLIENT(client)));
            auto self = static_cast<EdsSource*>(gself);
            self->m_client = E_CAL_CLIENT(client);
        }
    }

    void clear_view()
    {
        if (m_view != nullptr)
        {
            e_cal_client_view_stop(m_view, nullptr);
            const auto n_disconnected = g_signal_handlers_disconnect_by_data(m_view, this);
            g_warn_if_fail(n_disconnected == 3);
            g_clear_object(&m_view);
        }
    }

    static std::string create_time_range_sexp(const DateTime& begin, const DateTime& end)
    {
        const char * fmt {"%Y%m%dT%H%M%S"};
        const auto begin_str = begin.format(fmt);
        const auto end_str = end.format(fmt);
        return g_strdup_printf ("(occur-in-time-range? (make-time \"%s\") (make-time \"%s\"))",
                                begin_str.c_str(),
                                end_str.c_str());
    }

    void rebuild_view()
    {
        if (m_client != nullptr)
        {
            clear_view();

            const auto range = m_range.get();
            const auto sexp = create_time_range_sexp(range.first, range.second);
            g_message("creating filtered views: %s", sexp.c_str());
            e_cal_client_get_view (m_client, sexp.c_str(), m_cancellable, on_client_view_ready, this);
        }
    }

    static void on_client_view_ready (GObject* client, GAsyncResult* res, gpointer gself)
    {
        GError* error = nullptr;
        ECalClientView* view = nullptr;

        if (e_cal_client_get_view_finish (E_CAL_CLIENT(client), res, &view, &error))
        {
            // add the view to our collection
            e_cal_client_view_set_flags(view, E_CAL_CLIENT_VIEW_FLAGS_NOTIFY_INITIAL, NULL);
            e_cal_client_view_start(view, &error);
            g_debug("got a view for %s", e_cal_client_get_local_attachment_store(E_CAL_CLIENT(client)));
            auto self = static_cast<EdsSource*>(gself);
            self->m_view = view;

            g_signal_connect(view, "objects-added", G_CALLBACK(on_components_added_static), self);
            g_signal_connect(view, "objects-modified", G_CALLBACK(on_components_changed_static), self);
            g_signal_connect(view, "objects-removed", G_CALLBACK(on_components_removed_static), self);
        }
        else if(error != nullptr)
        {
            if (!g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
                g_warning("indicator-datetime cannot get View to EDS client: %s", error->message);

            g_error_free(error);
        }
    }

    static void on_components_added_static(ECalClientView* /*view*/, GSList* icalcomponents, gpointer gself)
    {
        g_debug("%s got %u components", G_STRFUNC, g_slist_length(icalcomponents));
        static_cast<EdsSource*>(gself)->on_components_added(icalcomponents);
    }

    static void on_components_changed_static(ECalClientView* /*view*/, GSList* icalcomponents, gpointer gself)
    {
        g_debug("%s got %u components", G_STRFUNC, g_slist_length(icalcomponents));
        static_cast<EdsSource*>(gself)->on_components_changed(icalcomponents);
    }

    void on_components_added(GSList* icalcomponents)
    {
        update_components(icalcomponents);
    }

    void on_components_changed(GSList* icalcomponents)
    {
        update_components(icalcomponents);
    }

    void update_components(GSList* icalcomponents)
    {
        bool changed = false;

        // walk through the icalcomponents that were added
        for(auto l=icalcomponents; l!=nullptr; l=l->next)
        {
            auto icc = static_cast<icalcomponent*>(l->data);
            const auto uid = icalcomponent_get_uid(icc);
            g_message("%s view notified of component '%s' {%s}", G_STRFUNC, uid, icalcomponent_as_ical_string(icc));

            auto icc2 = icalcomponent_new_clone(icc);
            auto component = e_cal_component_new_from_icalcomponent(icc2); // eats icc2
            cache_component(component);
            g_message ("now have %zu components", (size_t)m_components.size());
            g_object_unref(G_OBJECT(component));

            changed = true;
        }

        if (changed)
            update_appointments();
    }

    void cache_component(ECalComponent* component)
    {
        g_object_ref(G_OBJECT(component));
        auto deleter = [](ECalComponent* c){g_object_unref(G_OBJECT(c));};

        const gchar* uid;
        e_cal_component_get_uid(component, &uid);
        g_debug("%s caching '%s'", G_STRFUNC, uid);

        m_components[uid].reset(component, deleter);
    }

    static void on_components_removed_static(ECalClientView* /*view*/, GSList* component_ids, gpointer gself)
    {
        g_debug("%s got %u component ids", G_STRFUNC, g_slist_length(component_ids));
        static_cast<EdsSource*>(gself)->on_components_removed(component_ids);
    }

    void on_components_removed(GSList* component_ids)
    {
        bool changed = false;

        // walk through the ids of the removed components
        for(auto l=component_ids; l!=nullptr; l=l->next)
        {
            const auto uid = static_cast<ECalComponentId*>(l->data)->uid;
            g_debug("%s view notified of removed component '%s'", G_STRFUNC, uid);
            m_components.erase(uid);
            changed = true;
        }

        if (changed)
            update_appointments();
    }

    void update_appointments()
    {
        std::vector<Appointment> appointments;

        for(auto& kv : m_components)
        {
            const auto& uid = kv.first;
            const auto& component = kv.second.get();

            const auto vtype = e_cal_component_get_vtype(component);

            if ((vtype != E_CAL_COMPONENT_EVENT) && (vtype != E_CAL_COMPONENT_TODO))
                continue;

            auto status = ICAL_STATUS_NONE;
            e_cal_component_get_status(component, &status);
            if (!uid.empty() && (status != ICAL_STATUS_COMPLETED) && (status != ICAL_STATUS_CANCELLED))
            {
                Appointment appointment;

                // get the start time
                ECalComponentDateTime dtstart;
                e_cal_component_get_dtstart(component, &dtstart);
                appointment.begin = DateTime(dtstart);
                if (dtstart.value == nullptr)
                    continue;
                g_message ("%s (%s) --> %s", icaltime_as_ical_string(*dtstart.value), dtstart.tzid, appointment.begin.format("%F %T").c_str());

                // get the end time
                ECalComponentDateTime dtend;
                e_cal_component_get_dtend(component, &dtend);
                if (dtend.value) {
                    appointment.end = DateTime(dtend);
                    g_message ("%s (%s) --> %s", icaltime_as_ical_string(*dtend.value), dtend.tzid, appointment.end.format("%F %T").c_str());
                } else {
                    appointment.end = appointment.begin;
                }

                // get the summary
                ECalComponentText text;
                text.value = nullptr;
                e_cal_component_get_summary(component, &text);
                if (text.value)
                    appointment.summary = text.value;
                g_message ("[%s]", text.value);

                // other misc properties
                appointment.color = m_color;
                appointment.is_event = vtype == E_CAL_COMPONENT_EVENT;
                appointment.uid = uid;
                GList * alarm_uids = e_cal_component_get_alarm_uids(component);
                appointment.has_alarms = alarm_uids != nullptr;
                cal_obj_uid_list_free(alarm_uids);

                // walk the recurrence rules
                GSList* rrules = nullptr;
                e_cal_component_get_rrule_list(component, &rrules);
                const auto range = m_range.get();
                if (rrules == nullptr)
                {
                    appointments.push_back(appointment);
                }
                else
                {
                    const auto duration = appointment.end.to_unix() - appointment.begin.to_unix();

                    for (GSList* walk=rrules; walk!=nullptr; walk=walk->next)
                    {
                        struct icalrecurrencetype& recur = *static_cast<struct icalrecurrencetype*>(walk->data);
                        auto rit = icalrecur_iterator_new(recur, *dtstart.value);
                        for(;;)
                        {
                            struct icaltimetype next = icalrecur_iterator_next(rit);
                            if (icaltime_is_null_time(next))
                                break;

                            g_message ("%s", icaltime_as_ical_string(next));
                            auto tmpstart = dtstart;
                            tmpstart.value = &next;
                            const DateTime step(tmpstart);
                            if (!((range.first <= step) && (step <= range.second)))
                                continue;

                            g_message ("%s", step.format("%F %T").c_str());
                            auto tmp = appointment;
                            tmp.begin = step;
                            tmp.end = tmp.begin.add_full(0,0,0,0,0,duration);
                            appointments.push_back(tmp);
                        }
                        icalrecur_iterator_free(rit);
                    }
                }
                e_cal_component_free_datetime(&dtstart);
                e_cal_component_free_datetime(&dtend);
            }
        }

        std::sort(appointments.begin(),
                  appointments.end(),
                  [](const Appointment& a, const Appointment& b){return a.begin < b.begin;});

        for(const auto& appointment : appointments)
            g_message ("%p [%s...%s] %s", (void*)this, appointment.begin.format("%F %T").c_str(), appointment.end.format("%F %T").c_str(), appointment.summary.c_str());

        g_message("setting m-appointments to hold %zu", size_t{appointments.size()});
        m_appointments.set(appointments);
    }
  
    const ESource* m_source;
    const ECalClientSourceType m_source_type;
    std::string m_color;
    ECalClient* m_client {nullptr};
    ECalClientView* m_view {nullptr};
    GCancellable* m_cancellable {nullptr};
    std::map<std::string,std::shared_ptr<ECalComponent>> m_components;
    core::Property<std::vector<Appointment>> m_appointments;
    core::Property<std::pair<DateTime,DateTime>>& m_range;
};

class EdsEngine::Impl
{
public:

    Impl(EdsEngine& owner):
        m_owner(owner),
        m_cancellable(g_cancellable_new()),
        m_range(std::pair<DateTime,DateTime>(DateTime{time_t{0}},DateTime{time_t{0}}))
    {
        //DateTime zero{time_t{0}};
        //m_range.set(std::pair<DateTime,DateTime>(zero,zero));
        e_source_registry_new(m_cancellable, on_source_registry_ready, this);
    }

    ~Impl()
    {
        g_cancellable_cancel(m_cancellable);
        g_clear_object(&m_cancellable);

        while(!m_sources.empty())
            remove_source(*m_sources.begin());

        if (m_rebuild_tag)
            g_source_remove(m_rebuild_tag);

        if (m_source_registry)
            g_signal_handlers_disconnect_by_data(m_source_registry, this);
        g_clear_object(&m_source_registry);
    }

    core::Property<std::vector<Appointment>>& appointments()
    {
        return m_appointments;
    }

    void set_range(const DateTime& begin, const DateTime& end)
    {
        m_range.set(std::pair<DateTime,DateTime>(begin, end));
    }

private:

    void rebuild_now()
    {
        std::vector<Appointment> appointments;

        for (auto& eds_source : m_test)
        {
            const auto& a = eds_source->appointments().get();
            appointments.insert (appointments.end(), a.begin(), a.end());
        }
        std::sort(appointments.begin(),
                  appointments.end(),
                  [](const Appointment& a, const Appointment& b){return a.begin < b.begin;});
g_message("%s... %zu appointments", G_STRFUNC, appointments.size());
        m_appointments.set(appointments);
    }

    std::string create_time_range_sexp(const DateTime& begin, const DateTime& end)
    {
        const char * fmt {"%Y%m%dT%H%M%S"};
        const auto begin_str = begin.format(fmt);
        const auto end_str = end.format(fmt);
        return g_strdup_printf ("(occur-in-time-range? (make-time \"%s\") (make-time \"%s\"))",
                                begin_str.c_str(),
                                end_str.c_str());
    }

    static gboolean rebuild_now_static (gpointer gself)
    {
g_message("%s", G_STRFUNC);
        auto self = static_cast<Impl*>(gself);
        self->m_rebuild_tag = 0;
        self->m_rebuild_deadline = 0;
        self->rebuild_now();
        return G_SOURCE_REMOVE;
    }

    void rebuild_soon()
    {
g_message("%s", G_STRFUNC);
        static constexpr int MIN_BATCH_SEC = 1;
        static constexpr int MAX_BATCH_SEC = 60;
        static_assert(MIN_BATCH_SEC <= MAX_BATCH_SEC, "bad boundaries");

        const auto now = time(nullptr);

        if (m_rebuild_deadline == 0) // first pass
        {
            m_rebuild_deadline = now + MAX_BATCH_SEC;
            m_rebuild_tag = g_timeout_add_seconds(MIN_BATCH_SEC, rebuild_now_static, this);
        }
        else if (now < m_rebuild_deadline)
        {
            g_source_remove (m_rebuild_tag);
            m_rebuild_tag = g_timeout_add_seconds(MIN_BATCH_SEC, rebuild_now_static, this);
        }
    }

    static void on_source_registry_ready(GObject* /*source*/, GAsyncResult* res, gpointer gself)
    {
        GError * error = nullptr;
        auto r = e_source_registry_new_finish(res, &error);
        if (error != nullptr)
        {
            if (!g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
                g_warning("indicator-datetime cannot show EDS appointments: %s", error->message);

            g_error_free(error);
        }
        else
        {
            g_signal_connect(r, "source-added",    G_CALLBACK(on_source_added),    gself);
            g_signal_connect(r, "source-removed",  G_CALLBACK(on_source_removed),  gself);
            g_signal_connect(r, "source-disabled", G_CALLBACK(on_source_disabled), gself);
            g_signal_connect(r, "source-enabled",  G_CALLBACK(on_source_enabled),  gself);

            auto self = static_cast<Impl*>(gself);
            self->m_source_registry = r;
            self->add_sources_by_extension(E_SOURCE_EXTENSION_CALENDAR);
            self->add_sources_by_extension(E_SOURCE_EXTENSION_TASK_LIST);
        }
    }

    void add_sources_by_extension(const char* extension)
    {
        auto& r = m_source_registry;
        auto sources = e_source_registry_list_sources(r, extension);
        for (auto l=sources; l!=nullptr; l=l->next)
            on_source_added(r, E_SOURCE(l->data), this);
        g_list_free_full(sources, g_object_unref);
    }

    static void on_source_added(ESourceRegistry* registry, ESource* source, gpointer gself)
    {
        auto self = static_cast<Impl*>(gself);

        self->m_sources.insert(E_SOURCE(g_object_ref(source)));

        if (e_source_get_enabled(source))
            on_source_enabled(registry, source, gself);
    }

    static void on_source_enabled(ESourceRegistry* /*registry*/, ESource* source, gpointer gself)
    {
        ECalClientSourceType type;
        auto self = static_cast<Impl*>(gself);
        const auto source_uid = e_source_get_uid(source);

        if (EdsSource::get_preferred_type(source, type))
        {
            g_debug("%s connecting a client to source %s", G_STRFUNC, source_uid);
            auto test = new EdsSource(source, type, self->m_range);

            test->appointments().changed().connect([self](const std::vector<Appointment>&) {
                g_message("--> %s", G_STRFUNC);
                self->rebuild_soon();
            });

            self->m_test.insert(test);
        }
        else
        {
            g_debug("%s not using source %s -- no tasks/calendar", G_STRFUNC, source_uid);

        }
    }

    static void on_source_disabled(ESourceRegistry* /*registry*/, ESource* source, gpointer gself)
    {
        static_cast<Impl*>(gself)->disable_source(source);
    }
    void disable_source(ESource*)
    {
/* TODO fixme */
#if 0
        // if an ECalClientView is associated with this source, remove it
        auto vit = m_views.find(source);
        if (vit != m_views.end())
            remove_view (vit->first, vit->second);

        // if an ECalClient is associated with this source, remove it
        auto cit = m_clients.find(source);
        if (cit != m_clients.end())
        {
            auto& client = cit->second;
            g_object_unref(client);
            m_clients.erase(cit);
            set_dirty_soon();
        }
#endif
    }

    static void on_source_removed(ESourceRegistry* /*registry*/, ESource* source, gpointer gself)
    {
        static_cast<Impl*>(gself)->remove_source(source);
    }
    void remove_source(ESource* source)
    {
        disable_source(source);

        auto sit = m_sources.find(source);
        if (sit != m_sources.end())
        {
            g_object_unref(*sit);
            m_sources.erase(sit);
            rebuild_soon();
        }
    }

private:

    EdsEngine& m_owner;
    GCancellable* m_cancellable{nullptr};
    ESourceRegistry* m_source_registry{nullptr};
    std::set<ESource*> m_sources;
    std::set<EdsSource*> m_test;
    guint m_rebuild_tag{0};
    time_t m_rebuild_deadline{0};
    core::Property<std::pair<DateTime,DateTime>> m_range;
    core::Property<std::vector<Appointment>> m_appointments;
};

/***
****
***/

EdsEngine::EdsEngine():
    p(new Impl(*this))
{
}

EdsEngine::~EdsEngine() =default;

core::Property<std::vector<Appointment>>& EdsEngine::appointments()
{
    return p->appointments();
}

void EdsEngine::set_range(const DateTime& begin, const DateTime& end)
{
    p->set_range(begin, end);
}

/***
****
***/

} // namespace datetime
} // namespace indicator
} // namespace unity
