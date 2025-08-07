#include <geanyplugin.h>
#include <geany.h>
#include <glib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <curl/curl.h>

static time_t last_heartbeat = 0;
static gchar *last_file = NULL;
static const gint HEARTBEAT_FREQUENCY = 120;
static const gchar *WAKATIME_CLI_VERSION = "v1.102.1";
static gboolean cli_download_in_progress = FALSE;

typedef struct {
    GeanyPlugin *plugin;
    gulong doc_open_id;
    gulong doc_save_id;
    gulong doc_activate_id;
} WakaTimeData;

typedef struct {
    FILE *fp;
} DownloadData;

static size_t write_data(void *ptr, size_t size, size_t nmemb, DownloadData *data)
{
    if (data->fp) {
        return fwrite(ptr, size, nmemb, data->fp);
    }
    return 0;
}

static gboolean download_wakatime_cli(void)
{
    if (cli_download_in_progress) {
        return FALSE;
    }

    cli_download_in_progress = TRUE;

    const gchar *home = g_get_home_dir();
    gchar *wakatime_dir = g_build_filename(home, ".wakatime", NULL);
    gchar *cli_path = g_build_filename(wakatime_dir, "wakatime-cli", NULL);
    gchar *temp_path = g_build_filename(wakatime_dir, "wakatime-cli.tmp", NULL);

    g_mkdir_with_parents(wakatime_dir, 0755);

    CURL *curl = curl_easy_init();
    if (!curl) {
        g_warning("WakaTime: Failed to initialize curl");
        goto cleanup;
    }

    gchar *url;
#ifdef __APPLE__
    url = g_strdup_printf("https://github.com/wakatime/wakatime-cli/releases/download/%s/wakatime-cli-darwin-amd64", WAKATIME_CLI_VERSION);
#elif defined(__linux__)
    url = g_strdup_printf("https://github.com/wakatime/wakatime-cli/releases/download/%s/wakatime-cli-linux-amd64", WAKATIME_CLI_VERSION);
#else
    url = g_strdup_printf("https://github.com/wakatime/wakatime-cli/releases/download/%s/wakatime-cli-linux-amd64", WAKATIME_CLI_VERSION);
#endif

    DownloadData data;
    data.fp = fopen(temp_path, "wb");
    if (!data.fp) {
        g_warning("WakaTime: Failed to create temp file");
        curl_easy_cleanup(curl);
        goto cleanup;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &data);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "geany-wakatime/1.0.0");

    CURLcode res = curl_easy_perform(curl);
    fclose(data.fp);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        g_warning("WakaTime: Failed to download CLI: %s", curl_easy_strerror(res));
        unlink(temp_path);
        goto cleanup;
    }

    if (rename(temp_path, cli_path) != 0) {
        g_warning("WakaTime: Failed to move temp file");
        unlink(temp_path);
        goto cleanup;
    }

    chmod(cli_path, 0755);

    cleanup:
    g_free(url);
    g_free(wakatime_dir);
    g_free(cli_path);
    g_free(temp_path);
    cli_download_in_progress = FALSE;

    return TRUE;
}

static gchar* get_wakatime_cli_path(void)
{
    const gchar *home = g_get_home_dir();
    gchar *wakatime_dir = g_build_filename(home, ".wakatime", NULL);
    gchar *cli_path = g_build_filename(wakatime_dir, "wakatime-cli", NULL);

    if (!g_file_test(cli_path, G_FILE_TEST_IS_EXECUTABLE)) {
        g_free(cli_path);
        cli_path = g_find_program_in_path("wakatime");

        if (!cli_path) {
            g_print("WakaTime: CLI not found, attempting to download...\n");
            download_wakatime_cli();
            cli_path = g_build_filename(wakatime_dir, "wakatime-cli", NULL);
            if (!g_file_test(cli_path, G_FILE_TEST_IS_EXECUTABLE)) {
                g_free(cli_path);
                cli_path = NULL;
            }
        }
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

static gchar* get_project_name(const gchar *file_path)
{
    gchar *project_dir = g_path_get_dirname(file_path);
    gchar *current = project_dir;

    while (current && g_strcmp0(current, "/") != 0) {
        gchar *git_dir = g_build_filename(current, ".git", NULL);
        if (g_file_test(git_dir, G_FILE_TEST_IS_DIR)) {
            g_free(git_dir);
            gchar *project_name = g_path_get_basename(current);
            g_free(project_dir);
            return project_name;
        }
        g_free(git_dir);

        gchar *parent = g_path_get_dirname(current);
        if (g_strcmp0(parent, current) == 0) {
            g_free(parent);
            break;
        }
        if (current != project_dir) g_free(current);
        current = parent;
    }

    if (current != project_dir) g_free(current);
    g_free(project_dir);
    return NULL;
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
    g_ptr_array_add(argv, g_strdup("--plugin"));
    g_ptr_array_add(argv, g_strdup("geany-wakatime/1.0.0"));

    gchar *project = get_project_name(file_path);
    if (project) {
        g_ptr_array_add(argv, g_strdup("--project"));
        g_ptr_array_add(argv, project);
    }

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
