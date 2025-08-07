#include <geanyplugin.h>
static void item_activate_cb(GtkMenuItem *menuitem, gpointer user_data)
{
    dialogs_show_msgbox(GTK_MESSAGE_INFO, "Hello World");
}
static gboolean hello_init(GeanyPlugin *plugin, gpointer pdata)
{
    GtkWidget *main_menu_item;
    // Create a new menu item and show it
    main_menu_item = gtk_menu_item_new_with_mnemonic("Hello World");
    gtk_widget_show(main_menu_item);
    gtk_container_add(GTK_CONTAINER(plugin->geany_data->main_widgets->tools_menu),
            main_menu_item);
    g_signal_connect(main_menu_item, "activate",
            G_CALLBACK(item_activate_cb), NULL);
    geany_plugin_set_data(plugin, main_menu_item, NULL);
    return TRUE;
}
static void hello_cleanup(GeanyPlugin *plugin, gpointer pdata)
{
    GtkWidget *main_menu_item = (GtkWidget *) pdata;
    gtk_widget_destroy(main_menu_item);
}
G_MODULE_EXPORT
void geany_load_module(GeanyPlugin *plugin)
{
    plugin->info->name = "WakaTime";
    plugin->info->description = "Just another tool to say hello world";
    plugin->info->version = "1.0";
    plugin->info->author = "John Doe <john.doe@example.org>";
    plugin->funcs->init = hello_init;
    plugin->funcs->cleanup = hello_cleanup;
    GEANY_PLUGIN_REGISTER(plugin, 225);
}
