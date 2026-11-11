#ifndef NEWS_WIDGET_H
#define NEWS_WIDGET_H

#include <gtk/gtk.h>

#include <memory>
#include <string>
#include <vector>

struct NewsItem {
    std::string title;
    std::string time;
    std::string url;
};

class NewsWidget {
public:
    struct SharedState;
    void set_language(const std::string& lang);
    std::string language = "it";

    static NewsWidget* create();
    GtkWidget* get_widget() const;
    void free_resources();

private:
    NewsWidget() = default;
    ~NewsWidget() = default;

    static void on_refresh_clicked(GtkButton*, gpointer);
    static void on_more_clicked(GtkButton*, gpointer);
    static void on_fetch_complete(GObject*, GAsyncResult*, gpointer);
    static void fetch_worker(GTask*, gpointer, gpointer, GCancellable*);

    void start_fetch();
    void render_news(const std::vector<NewsItem>&);

    GtkWidget* container = nullptr;
    GtkWidget* list_box = nullptr;
    GtkWidget* refresh_button = nullptr;
    GtkWidget* lbl_title = nullptr;
    GtkWidget* more_button = nullptr;

    std::shared_ptr<SharedState> state;
};

#endif
