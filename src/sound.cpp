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

#include <notifications/sound.h>

#include <gst/gst.h>

#include <mutex> // std::call_once()

/* PulseImpl */
#include <sys/types.h>
#include <unistd.h>
#include <sstream>
#include <cstring>
#include <pulse/pulseaudio.h>

namespace unity {
namespace indicator {
namespace notifications {

/***
****
***/

/**
 * Changes pulseaudio output to speakers when needed
 */
class Sound::PulseImpl
{
public:

    PulseImpl()
        : m_mainloop(0),
          m_context(0),
          m_default_sink_name(""),
          m_old_active_port_name("")

    {
        m_mainloop = pa_threaded_mainloop_new();
        if (m_mainloop == 0) {
            g_warning("Unable to create pulseaudio mainloop");
            m_mainloop = 0;
            return;
        }

        if (pa_threaded_mainloop_start(m_mainloop) != 0) {
            g_warning("Unable to start pulseaudio mainloop");
            pa_threaded_mainloop_free(m_mainloop);
            m_mainloop = 0;
            return;
        }

        create_context();
    }

    pa_threaded_mainloop *mainloop() { return m_mainloop; }

    void handle_server_info(const pa_server_info *info)
    {
        m_default_sink_name = info->default_sink_name;
    }

    void handle_sink_info(const pa_sink_info *info)
    {
        if (m_set_preferred_sink_port)
            internal_set_preferred_sink_port(info);
    }

    bool set_preferred_sink_port()
    {
        m_set_preferred_sink_port = true;

        if (!create_context())
            return false;

        pa_threaded_mainloop_lock(m_mainloop);

        pa_operation *o;

        /* Get default sink name */
        o = pa_context_get_server_info(m_context, get_server_info_callback, this);
        if (!handle_operation(o, "pa_context_get_server_info"))
            return false;

        /* Get default sink info */
        o = pa_context_get_sink_info_by_name(m_context,
                                             m_default_sink_name.c_str(),
                                             get_sink_info_by_name_callback,
                                             this);
        if (!handle_operation(o, "pa_context_get_sink_info_by_name"))
            return false;

        /* If needed, change default sink output port */
        if (!m_preferred_port_name.empty())
        {
            g_debug("Setting pulseaudio sink '%s' port from '%s' to '%s'", m_default_sink_name.c_str(), m_old_active_port_name.c_str(), m_preferred_port_name.c_str());
            o = pa_context_set_sink_port_by_name(m_context,
                                                 m_default_sink_name.c_str(),
                                                 m_preferred_port_name.c_str(),
                                                 success_callback, this);
            if (handle_operation(o, "pa_context_set_sink_port_by_name"))
            {
                /* Discard m_preferred_port_name to avoid previous operation when port is already set */
                m_preferred_port_name = "";
            }
            else
                return false;
        }

        pa_threaded_mainloop_unlock(m_mainloop);

        return true;
    }

    bool restore_sink_port()
    {
        /* If default sink port was changed, restore it */
        if (m_context &&
            m_set_preferred_sink_port &&
            !m_old_active_port_name.empty())
        {
            pa_threaded_mainloop_lock(m_mainloop);

            g_debug("Restoring pulseaudio sink '%s' port to '%s'", m_default_sink_name.c_str(), m_old_active_port_name.c_str());
            m_set_preferred_sink_port = false;

            pa_operation *o;
            o = pa_context_set_sink_port_by_name(m_context,
                                                 m_default_sink_name.c_str(),
                                                 m_old_active_port_name.c_str(),
                                                 success_callback, this);
            if (!handle_operation(o, "pa_context_set_sink_port_by_name"))
                return false;

            m_old_active_port_name = "";

            pa_threaded_mainloop_unlock(m_mainloop);
        }

        return true;
    }

    ~PulseImpl()
    {
        restore_sink_port();
        release_context();

        if (m_mainloop) {
            pa_threaded_mainloop_stop(m_mainloop);
            pa_threaded_mainloop_free(m_mainloop);
            m_mainloop = 0;
        }
    }


private:
    static void success_callback(pa_context *context, int success, void *userdata)
    {
        (void)context; //unused
        (void)success; //unused

        PulseImpl *self = static_cast<PulseImpl *>(userdata);
        pa_threaded_mainloop_signal(self->mainloop(), 0);
    }

    static void set_state_callback(pa_context *context, void *userdata)
    {
        (void)context; //unused

        PulseImpl *self = static_cast<PulseImpl *>(userdata);
        pa_threaded_mainloop_signal(self->mainloop(), 0);
    }

    static void get_server_info_callback(pa_context *context, const pa_server_info *info, void *userdata)
    {
        (void)context; //unused

        PulseImpl *self = static_cast<PulseImpl *>(userdata);
        self->handle_server_info(info);
        pa_threaded_mainloop_signal(self->mainloop(), 0);
    }

    static void get_sink_info_by_name_callback(pa_context *context, const pa_sink_info *info, int eol, void *userdata)
    {
        (void)context; //unused

        if (eol)
            return;

        PulseImpl *self = static_cast<PulseImpl *>(userdata);
        self->handle_sink_info(info);
        pa_threaded_mainloop_signal(self->mainloop(), 0);
    }

    bool create_context()
    {
        bool keepGoing = true;
        bool connected = false;

        if (m_context)
            return true;

        pa_mainloop_api *m_mainloop_api;
        m_mainloop_api = pa_threaded_mainloop_get_api(m_mainloop);

        pa_threaded_mainloop_lock(m_mainloop);

        std::stringstream pid;
        pid << ::getpid();
        std::string name = "QtmPulseContext:";
        name.append(pid.str());
        m_context = pa_context_new(m_mainloop_api, name.c_str());
        if (!m_context) {
            g_critical("Unable to create new pulseaudio context");
            pa_threaded_mainloop_unlock(m_mainloop);
            return false;
        }

        pa_context_set_state_callback(m_context, set_state_callback, this);
        if (pa_context_connect(m_context, NULL, PA_CONTEXT_NOFLAGS, NULL) < 0) {
            g_critical("Unable to create a connection to the pulseaudio context");
            pa_threaded_mainloop_unlock(m_mainloop);
            release_context();
            return false;
        }

        g_debug("Connecting to the pulseaudio context");
        pa_threaded_mainloop_wait(m_mainloop);
        while (keepGoing) {
            switch (pa_context_get_state(m_context)) {
                case PA_CONTEXT_CONNECTING:
                case PA_CONTEXT_AUTHORIZING:
                case PA_CONTEXT_SETTING_NAME:
                    break;

                case PA_CONTEXT_READY:
                    g_debug("Pulseaudio connection established.");
                    keepGoing = false;
                    connected = true;
                    break;

                case PA_CONTEXT_TERMINATED:
                    g_critical("Pulseaudio context terminated.");
                    keepGoing = false;
                    break;

                case PA_CONTEXT_FAILED:
                default:
                    g_critical("Pulseaudio connection failure: %s", pa_strerror(pa_context_errno(m_context)));
                    keepGoing = false;
            }

            if (keepGoing) {
                pa_threaded_mainloop_wait(m_mainloop);
            }
        }

        if (!connected) {
            if (m_context) {
                pa_context_unref(m_context);
                m_context = 0;
            }
        }

        pa_threaded_mainloop_unlock(m_mainloop);

        return connected;
    }

    bool handle_operation(pa_operation *operation, const char *func_name)
    {
        if (!operation) {
            g_critical("'%s' failed (lost pulseaudio connection?)", func_name);
            /* Free resources so it can retry a new connection during next operation */
            pa_threaded_mainloop_unlock(m_mainloop);
            release_context();
            return false;
        }

        while (pa_operation_get_state(operation) == PA_OPERATION_RUNNING)
            pa_threaded_mainloop_wait(m_mainloop);

        pa_operation_unref(operation);

        return true;
    }

    void release_context()
    {
        if (m_context) {
            pa_threaded_mainloop_lock(m_mainloop);
            pa_context_disconnect(m_context);
            pa_context_unref(m_context);
            pa_threaded_mainloop_unlock(m_mainloop);
            m_context = 0;
        }
    }

    bool internal_set_preferred_sink_port(const pa_sink_info *info)
    {
        /* Prefer speakers over headphones when playing audio (LP: #1364647) */
        pa_sink_port_info* active_port = info->active_port;
        pa_sink_port_info* speaker_port = nullptr;
        pa_sink_port_info* speaker_wired_headphone_port = nullptr;
        pa_sink_port_info* preferred_port = nullptr;

        for (unsigned int i = 0; i < info->n_ports; i++)
        {
            if (strcmp(info->ports[i]->name, "output-speaker") == 0)
                speaker_port = info->ports[i];
            else if (strcmp(info->ports[i]->name, "output-speaker+wired_headphone") == 0)
                speaker_wired_headphone_port = info->ports[i];
        }

        if (speaker_wired_headphone_port != nullptr)
            preferred_port = speaker_wired_headphone_port;
        else
            preferred_port = speaker_port;

        if (preferred_port != nullptr &&
            strcmp(active_port->name, "output-speaker") != 0 &&
            active_port != preferred_port)
        {
            m_old_active_port_name = active_port->name;
            m_preferred_port_name = preferred_port->name;
        }

        return true;
    }

    /***
    ****
    ***/

    pa_threaded_mainloop *m_mainloop;
    pa_context *m_context;
    std::string m_default_sink_name;
    std::string m_old_active_port_name;
    std::string m_preferred_port_name;
    bool m_set_preferred_sink_port = false;
};

/**
 * Plays a sound, possibly looping.
 */
class Sound::Impl
{
public:

    Impl(const std::string& role,
         const std::string& uri,
         unsigned int volume,
         bool loop):
        m_role(role),
        m_uri(uri),
        m_volume(volume),
        m_loop(loop)
    {
        // init GST once
        static std::once_flag once;
        std::call_once(once, [](){
            GError* error = nullptr;
            if (!gst_init_check (nullptr, nullptr, &error))
            {
                g_critical("Unable to play alarm sound: %s", error->message);
                g_error_free(error);
            }
        });

        m_play = gst_element_factory_make("playbin", "play");

        auto bus = gst_pipeline_get_bus(GST_PIPELINE(m_play));
        m_watch_source = gst_bus_add_watch(bus, bus_callback, this);
        gst_object_unref(bus);

        g_debug("Playing '%s'", m_uri.c_str());
        g_object_set(G_OBJECT (m_play), "uri", m_uri.c_str(),
                                        "volume", get_volume(),
                                        nullptr);
        gst_element_set_state (m_play, GST_STATE_PLAYING);
    }

    ~Impl()
    {
        if (m_pulse_timeout != 0)
            g_source_remove(m_pulse_timeout);

        if (m_pulse != nullptr)
            delete m_pulse;

        g_source_remove(m_watch_source);

        if (m_play != nullptr)
        {
            gst_element_set_state (m_play, GST_STATE_NULL);
            g_clear_pointer (&m_play, gst_object_unref);
        }
    }

private:

    // convert settings range [1..100] to gst playbin's range is [0...1.0]
    gdouble get_volume() const
    {
        constexpr int in_range_lo = 1;
        constexpr int in_range_hi = 100;
        const double in = CLAMP(m_volume, in_range_lo, in_range_hi);
        const double pct = (in - in_range_lo) / (in_range_hi - in_range_lo);

        constexpr double out_range_lo = 0.0; 
        constexpr double out_range_hi = 1.0; 
        return out_range_lo + (pct * (out_range_hi - out_range_lo));
    }

    static gboolean pulse_timer_callback(gpointer gself)
    {
        auto self = static_cast<Impl*>(gself);

        if (self->m_pulse == nullptr)
            self->m_pulse = new PulseImpl();

        /* Set preferred sink port */
        self->m_pulse->set_preferred_sink_port();

        self->m_pulse_timeout = 0;
        return false;
    }

    static gboolean bus_callback(GstBus*, GstMessage* msg, gpointer gself)
    {
        auto self = static_cast<Impl*>(gself);
        const auto message_type = GST_MESSAGE_TYPE(msg);

        if ((message_type == GST_MESSAGE_EOS) && (self->m_loop))
        {
            gst_element_seek(self->m_play,
                             1.0,
                             GST_FORMAT_TIME,
                             GST_SEEK_FLAG_FLUSH,
                             GST_SEEK_TYPE_SET,
                             0,
                             GST_SEEK_TYPE_NONE,
                             (gint64)GST_CLOCK_TIME_NONE);
        }
        else if (message_type == GST_MESSAGE_STREAM_START)
        {
            /* Set the media role and pulse timer if audio sink is pulsesink */
            GstElement *audio_sink = nullptr;
            g_object_get(self->m_play, "audio-sink", &audio_sink, nullptr);
            if (audio_sink)
            {
                GstPluginFeature *feature = nullptr;
                feature = GST_PLUGIN_FEATURE_CAST(GST_ELEMENT_GET_CLASS(audio_sink)->elementfactory);
                if (feature && g_strcmp0(gst_plugin_feature_get_name(feature), "pulsesink") == 0)
                {
                    auto role_str = g_strdup_printf("props,media.role=%s", self->m_role.c_str());
                    GstStructure *props = gst_structure_from_string(role_str, nullptr);
                    g_object_set(audio_sink, "stream-properties", props, nullptr);
                    gst_structure_free(props);
                    g_free(role_str);

                    /* Set preferred sink port after 5 seconds */
                    if (self->m_pulse == nullptr &&
                        self->m_pulse_timeout == 0)
                        self->m_pulse_timeout = g_timeout_add_seconds(5, pulse_timer_callback, self);
                }
                gst_object_unref(audio_sink);
            }
        }

        return G_SOURCE_CONTINUE; // keep listening
    }

    /***
    ****
    ***/

    const std::string m_role;
    const std::string m_uri;
    const unsigned int m_volume;
    const bool m_loop;
    guint m_watch_source = 0;
    GstElement* m_play = nullptr;
    guint m_pulse_timeout = 0;
    PulseImpl* m_pulse = nullptr;
};

Sound::Sound(const std::string& role, const std::string& uri, unsigned int volume, bool loop):
  impl (new Impl(role, uri, volume, loop))
{
}

Sound::~Sound()
{
}

/***
****
***/

} // namespace notifications
} // namespace indicator
} // namespace unity
