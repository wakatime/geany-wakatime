#include <geanyplugin.h>
#include <geany.h>
#include <glib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

static time_t last_heartbeat = 0;
static gchar *last_file = NULL;
static const gint HEARTBEAT_FREQUENCY = 120;

typedef struct {
    GeanyPlugin *plugin;
    gulong doc_open_id;
    gulong doc_save_id;
    gulong doc_activate_id;
} WakaTimeData;

static gchar* get_wakatime_cli_path(void)
{
    const gchar *home = g_get_home_dir();
    gchar *wakatime_dir = g_build_filename(home, ".wakatime", NULL);
    gchar *cli_path = g_build_filename(wakatime_dir, "wakatime-cli", NULL);

    if (!g_file_test(cli_path, G_FILE_TEST_IS_EXECUTABLE)) {
        g_free(cli_path);
        cli_path = g_find_program_in_path("wakatime");
    }

    g_free(wakatime_dir);
    return cli_path;
}

static gboolean has_api_key(void)
{
    const gchar *home = g_get_home_dir();
    gchar *config_path = g_build_filename(home, ".wakatime.cfg", NULL);
    gboolean exists = g_file_test(config_path, G_FILE_TEST_EXISTS);
    g_free(config_path);
    return exists;
}

static void send_heartbeat(const gchar *file_path, gboolean is_write)
{
    time_t now = time(NULL);

    if (!file_path) return;

    if (last_file && g_strcmp0(last_file, file_path) == 0 &&
        (now - last_heartbeat) < HEARTBEAT_FREQUENCY && !is_write) {
        return;
    }

    gchar *cli_path = get_wakatime_cli_path();
    if (!cli_path) {
        g_warning("WakaTime: wakatime-cli not found");
        return;
    }

    if (!has_api_key()) {
        g_warning("WakaTime: API key not configured in ~/.wakatime.cfg");
        g_free(cli_path);
        return;
    }

    GPtrArray *argv = g_ptr_array_new();
    g_ptr_array_add(argv, cli_path);
    g_ptr_array_add(argv, g_strdup("--entity"));
    g_ptr_array_add(argv, g_strdup(file_path));

    if (is_write) {
        g_ptr_array_add(argv, g_strdup("--write"));
    }

    g_ptr_array_add(argv, NULL);

    GError *error = NULL;
    g_spawn_async(NULL, (gchar**)argv->pdata, NULL,
                  G_SPAWN_SEARCH_PATH | G_SPAWN_STDOUT_TO_DEV_NULL | G_SPAWN_STDERR_TO_DEV_NULL,
                  NULL, NULL, NULL, &error);

    if (error) {
        g_warning("WakaTime: Failed to execute wakatime-cli: %s", error->message);
        g_error_free(error);
    } else {
        last_heartbeat = now;
        g_free(last_file);
        last_file = g_strdup(file_path);
    }

    for (guint i = 0; i < argv->len - 1; i++) {
        g_free(g_ptr_array_index(argv, i));
    }
    g_ptr_array_free(argv, TRUE);
}

static void on_document_open(GObject *obj, GeanyDocument *doc, gpointer user_data)
{
    if (doc && doc->file_name) {
        send_heartbeat(doc->file_name, FALSE);
    }
}

static void on_document_save(GObject *obj, GeanyDocument *doc, gpointer user_data)
{
    if (doc && doc->file_name) {
        send_heartbeat(doc->file_name, TRUE);
    }
}

static void on_document_activate(GObject *obj, GeanyDocument *doc, gpointer user_data)
{
    if (doc && doc->file_name) {
        send_heartbeat(doc->file_name, FALSE);
    }
}

static gboolean wakatime_init(GeanyPlugin *plugin, gpointer pdata)
{
    WakaTimeData *data = g_new0(WakaTimeData, 1);
    data->plugin = plugin;

    data->doc_open_id = g_signal_connect(plugin->geany_data->object, "document-open",
                                        G_CALLBACK(on_document_open), data);
    data->doc_save_id = g_signal_connect(plugin->geany_data->object, "document-save",
                                        G_CALLBACK(on_document_save), data);
    data->doc_activate_id = g_signal_connect(plugin->geany_data->object, "document-activate",
                                            G_CALLBACK(on_document_activate), data);

    geany_plugin_set_data(plugin, data, NULL);

    GeanyDocument *current_doc = document_get_current();
    if (current_doc && current_doc->file_name) {
        send_heartbeat(current_doc->file_name, FALSE);
    }

    return TRUE;
}

static void wakatime_cleanup(GeanyPlugin *plugin, gpointer pdata)
{
    WakaTimeData *data = (WakaTimeData*)pdata;

    if (data) {
        g_signal_handler_disconnect(plugin->geany_data->object, data->doc_open_id);
        g_signal_handler_disconnect(plugin->geany_data->object, data->doc_save_id);
        g_signal_handler_disconnect(plugin->geany_data->object, data->doc_activate_id);
        g_free(data);
    }

    g_free(last_file);
    last_file = NULL;
}

G_MODULE_EXPORT
void geany_load_module(GeanyPlugin *plugin)
{
    plugin->info->name = "WakaTime";
    plugin->info->description = "Automatic time tracking for programmers using WakaTime";
    plugin->info->version = "1.0.0";
    plugin->info->author = "WakaTime";
    plugin->funcs->init = wakatime_init;
    plugin->funcs->cleanup = wakatime_cleanup;
    GEANY_PLUGIN_REGISTER(plugin, 225);
}
