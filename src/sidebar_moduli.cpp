#include <gtk/gtk.h>

struct SidebarApp {
    GtkWidget* window = nullptr;
    GtkWidget* main_box = nullptr;
    GtkWidget* weather = nullptr;
    GtkWidget* toggle_button = nullptr;
    bool weather_active = true;
};

static GtkWidget* create_weather_widget() {
    GtkWidget* frame = gtk_frame_new("Meteo");
    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);

    gtk_container_set_border_width(GTK_CONTAINER(box), 8);
    gtk_box_pack_start(GTK_BOX(box), gtk_label_new("Roma, Italia"),
                       FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), gtk_label_new("22°C - ☀️ Sereno"),
                       FALSE, FALSE, 0);

    gtk_container_add(GTK_CONTAINER(frame), box);
    return frame;
}

static void on_toggle_weather(GtkButton*, gpointer data) {
    auto* app = static_cast<SidebarApp*>(data);

    if (app->weather_active) {
        gtk_container_remove(GTK_CONTAINER(app->main_box), app->weather);
        app->weather = nullptr;
        gtk_button_set_label(GTK_BUTTON(app->toggle_button), "Mostra Meteo");
    } else {
        app->weather = create_weather_widget();
        gtk_box_pack_start(GTK_BOX(app->main_box), app->weather,
                           FALSE, FALSE, 5);
        gtk_box_reorder_child(GTK_BOX(app->main_box), app->weather, 2);
        gtk_widget_show_all(app->weather);
        gtk_button_set_label(GTK_BUTTON(app->toggle_button), "Rimuovi Meteo");
    }

    app->weather_active = !app->weather_active;
}

int main(int argc, char* argv[]) {
    gtk_init(&argc, &argv);

    SidebarApp app;

    app.window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(app.window), "Sidebar Modulare");
    gtk_window_set_default_size(GTK_WINDOW(app.window), 250, 500);
    g_signal_connect(app.window, "destroy", G_CALLBACK(gtk_main_quit), nullptr);

    app.main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_container_set_border_width(GTK_CONTAINER(app.main_box), 10);

    GtkWidget* header = gtk_label_new(nullptr);
    gtk_label_set_markup(GTK_LABEL(header), "<big><b>antiX Sidebar</b></big>");
    gtk_box_pack_start(GTK_BOX(app.main_box), header, FALSE, FALSE, 5);

    app.toggle_button = gtk_button_new_with_label("Rimuovi Meteo");
    g_signal_connect(app.toggle_button, "clicked",
                     G_CALLBACK(on_toggle_weather), &app);
    gtk_box_pack_start(GTK_BOX(app.main_box), app.toggle_button, FALSE, FALSE, 5);

    gtk_box_pack_start(GTK_BOX(app.main_box),
                       gtk_separator_new(GTK_ORIENTATION_HORIZONTAL),
                       FALSE, FALSE, 5);

    app.weather = create_weather_widget();
    gtk_box_pack_start(GTK_BOX(app.main_box), app.weather, FALSE, FALSE, 5);

    gtk_container_add(GTK_CONTAINER(app.window), app.main_box);
    gtk_widget_show_all(app.window);
    gtk_main();

    return 0;
}
