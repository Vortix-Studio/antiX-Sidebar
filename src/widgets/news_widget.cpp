#include "news_widget.h"

#include <curl/curl.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libintl.h>

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#define _(s) dgettext("antix-sidebar", s)

struct NewsWidget::SharedState {
    NewsWidget* widget = nullptr; // Accesso solo dal main thread GTK
    std::atomic<bool> fetching{false};
};

namespace {

constexpr std::size_t MAX_ITEMS = 3;
constexpr std::size_t MAX_BYTES = 64 * 1024;
constexpr long CONNECT_TIMEOUT_SECONDS = 3;
constexpr long TOTAL_TIMEOUT_SECONDS = 5;

constexpr char FEED_URL[] =
    "https://feeds.bbci.co.uk/news/world/rss.xml";

constexpr char MORE_URL[] =
    "https://www.bbc.com/news/world";

std::once_flag curl_init_flag;

struct FetchContext {
    std::shared_ptr<NewsWidget::SharedState> state;
};

std::size_t write_data(
    char* data,
    std::size_t size,
    std::size_t count,
    void* user_data)
{
    auto* buffer = static_cast<std::string*>(user_data);

    if (size != 0 && count > MAX_BYTES / size)
        return 0;

    const std::size_t bytes = size * count;

    if (bytes > MAX_BYTES - buffer->size())
        return 0;

    buffer->append(data, bytes);
    return bytes;
}

std::string node_content(xmlNode* node)
{
    xmlChar* content = xmlNodeGetContent(node);
    if (!content)
        return {};

    std::string result(
        reinterpret_cast<const char*>(content));

    xmlFree(content);
    return result;
}

bool is_http_url(const std::string& url)
{
    GError* error = nullptr;
    GUri* uri = g_uri_parse(url.c_str(), G_URI_FLAGS_NONE, &error);
    bool valid = false;

    if (uri) {
        const char* scheme = g_uri_get_scheme(uri);

        valid = scheme &&
            (g_ascii_strcasecmp(scheme, "http") == 0 ||
             g_ascii_strcasecmp(scheme, "https") == 0);

        g_uri_unref(uri);
    }

    if (error)
        g_error_free(error);

    return valid;
}

NewsItem parse_item(xmlNode* node)
{
    NewsItem item;

    for (xmlNode* child = node->children; child; child = child->next) {
        if (child->type != XML_ELEMENT_NODE)
            continue;

        if (xmlStrEqual(child->name, BAD_CAST "title")) {
            item.title = node_content(child);
        } else if (
            xmlStrEqual(child->name, BAD_CAST "pubDate") ||
            xmlStrEqual(child->name, BAD_CAST "updated") ||
            xmlStrEqual(child->name, BAD_CAST "published")) {
            item.time = node_content(child);
        } else if (xmlStrEqual(child->name, BAD_CAST "link")) {
            xmlChar* href = xmlGetProp(child, BAD_CAST "href");

            if (href) {
                item.url = reinterpret_cast<const char*>(href);
                xmlFree(href);
            } else {
                item.url = node_content(child);
            }
        }
    }

    return item;
}

void collect_items(xmlNode* node, std::vector<NewsItem>& items)
{
    for (xmlNode* current = node;
         current && items.size() < MAX_ITEMS;
         current = current->next) {
        if (current->type == XML_ELEMENT_NODE &&
            (xmlStrEqual(current->name, BAD_CAST "item") ||
             xmlStrEqual(current->name, BAD_CAST "entry"))) {
            NewsItem item = parse_item(current);

            if (!item.title.empty())
                items.push_back(std::move(item));

            continue;
        }

        collect_items(current->children, items);
    }
}

std::vector<NewsItem> download_feed()
{
    std::vector<NewsItem> items;

    std::call_once(curl_init_flag, [] {
        curl_global_init(CURL_GLOBAL_DEFAULT);
    });

    CURL* curl = curl_easy_init();
    if (!curl)
        return items;

    std::string buffer;

    curl_easy_setopt(curl, CURLOPT_URL, FEED_URL);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "antiX-NewsWidget/1.0");
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, CONNECT_TIMEOUT_SECONDS);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, TOTAL_TIMEOUT_SECONDS);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 3L);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);

    const CURLcode result = curl_easy_perform(curl);

    long status = 0;
    if (result == CURLE_OK)
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);

    curl_easy_cleanup(curl);

    if (result != CURLE_OK ||
        status < 200 ||
        status >= 300 ||
        buffer.empty()) {
        return items;
    }

    xmlDocPtr doc = xmlReadMemory(
        buffer.data(),
        static_cast<int>(buffer.size()),
        nullptr,
        nullptr,
        XML_PARSE_NONET | XML_PARSE_NOERROR | XML_PARSE_NOWARNING);

    if (!doc)
        return items;

    if (xmlNode* root = xmlDocGetRootElement(doc))
        collect_items(root, items);

    xmlFreeDoc(doc);
    return items;
}

void clear_box(GtkWidget* box)
{
    GList* children = gtk_container_get_children(GTK_CONTAINER(box));

    for (GList* child = children; child; child = child->next)
        gtk_widget_destroy(GTK_WIDGET(child->data));

    g_list_free(children);
}

void on_news_link_clicked(GtkButton* button, gpointer)
{
    const char* url = static_cast<const char*>(
        g_object_get_data(G_OBJECT(button), "news-url"));

    if (!url)
        return;

    GError* error = nullptr;
    g_app_info_launch_default_for_uri(url, nullptr, &error);

    if (error)
        g_error_free(error);
}

} // namespace

NewsWidget* NewsWidget::create()
{
    auto* self = new NewsWidget();

    self->state = std::make_shared<SharedState>();
    self->state->widget = self;

    self->container = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_name(self->container, "news-widget");

    GtkWidget* header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_widget_set_name(header, "news-header");

    self->lbl_title = gtk_label_new(_("News"));
    gtk_widget_set_name(self->lbl_title, "news-title");
    gtk_widget_set_halign(self->lbl_title, GTK_ALIGN_START);

    self->refresh_button = gtk_button_new_with_label("↻");
    gtk_widget_set_name(self->refresh_button, "news-refresh-btn");
    gtk_widget_set_tooltip_text(
        self->refresh_button,
        _("Refresh news"));

    gtk_box_pack_start(
        GTK_BOX(header), self->lbl_title, TRUE, TRUE, 0);
    gtk_box_pack_end(
        GTK_BOX(header), self->refresh_button, FALSE, FALSE, 0);

    gtk_box_pack_start(
        GTK_BOX(self->container), header, FALSE, FALSE, 0);

    g_signal_connect(
        self->refresh_button,
        "clicked",
        G_CALLBACK(NewsWidget::on_refresh_clicked),
        self);

    GtkWidget* scroll = gtk_scrolled_window_new(nullptr, nullptr);
    gtk_widget_set_name(scroll, "news-scroll");
    gtk_widget_set_size_request(scroll, -1, 100);

    gtk_scrolled_window_set_policy(
        GTK_SCROLLED_WINDOW(scroll),
        GTK_POLICY_NEVER,
        GTK_POLICY_AUTOMATIC);

    self->list_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_widget_set_name(self->list_box, "news-list");

    gtk_container_add(GTK_CONTAINER(scroll), self->list_box);
    gtk_box_pack_start(
        GTK_BOX(self->container), scroll, TRUE, TRUE, 0);

    self->more_button = gtk_button_new_with_label(_("More news ›"));
    gtk_widget_set_name(self->more_button, "news-more-link");
    gtk_widget_set_halign(self->more_button, GTK_ALIGN_START);

    g_signal_connect(
        self->more_button,
        "clicked",
        G_CALLBACK(NewsWidget::on_more_clicked),
        nullptr);

    gtk_box_pack_start(
        GTK_BOX(self->container), self->more_button, FALSE, FALSE, 0);

    self->start_fetch();
    return self;
}

GtkWidget* NewsWidget::get_widget() const
{
    return container;
}

void NewsWidget::set_language(const std::string& lang)
{
    language = lang;

    if (lbl_title)
        gtk_label_set_text(GTK_LABEL(lbl_title), _("News"));

    if (more_button)
        gtk_button_set_label(
            GTK_BUTTON(more_button),
            _("More news ›"));

    if (refresh_button)
        gtk_widget_set_tooltip_text(
            refresh_button,
            _("Refresh news"));
}

void NewsWidget::on_refresh_clicked(GtkButton*, gpointer data)
{
    auto* self = static_cast<NewsWidget*>(data);
    if (self)
        self->start_fetch();
}

void NewsWidget::on_more_clicked(GtkButton*, gpointer)
{
    GError* error = nullptr;
    g_app_info_launch_default_for_uri(MORE_URL, nullptr, &error);

    if (error)
        g_error_free(error);
}

void NewsWidget::start_fetch()
{
    if (!state)
        return;

    bool expected = false;
    if (!state->fetching.compare_exchange_strong(expected, true))
        return;

    auto* context = new FetchContext{state};

    GTask* task = g_task_new(
        nullptr,
        nullptr,
        NewsWidget::on_fetch_complete,
        context);

    g_task_run_in_thread(task, NewsWidget::fetch_worker);
    g_object_unref(task);
}

void NewsWidget::fetch_worker(
    GTask* task,
    gpointer,
    gpointer,
    GCancellable*)
{
    auto* items = new std::vector<NewsItem>(download_feed());

    g_task_return_pointer(
        task,
        items,
        [](gpointer data) {
            delete static_cast<std::vector<NewsItem>*>(data);
        });
}

void NewsWidget::on_fetch_complete(
    GObject*,
    GAsyncResult* result,
    gpointer user_data)
{
    std::unique_ptr<FetchContext> context(
        static_cast<FetchContext*>(user_data));

    GError* error = nullptr;
    auto* result_items = static_cast<std::vector<NewsItem>*>(
        g_task_propagate_pointer(G_TASK(result), &error));

    // g_task_propagate_pointer transfiere la propiedad al llamador.
    std::unique_ptr<std::vector<NewsItem>> items(result_items);

    if (context->state->widget && items)
        context->state->widget->render_news(*items);

    if (error)
        g_error_free(error);

    context->state->fetching.store(false);
}

void NewsWidget::render_news(const std::vector<NewsItem>& items)
{
    if (!list_box)
        return;

    clear_box(list_box);

    if (items.empty()) {
        GtkWidget* label = gtk_label_new(_("News not available"));
        gtk_widget_set_name(label, "news-empty-label");
        gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
        gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
        gtk_widget_set_halign(label, GTK_ALIGN_START);

        gtk_box_pack_start(
            GTK_BOX(list_box), label, FALSE, FALSE, 2);
    } else {
        for (const NewsItem& item : items) {
            GtkWidget* row = gtk_box_new(GTK_ORIENTATION_VERTICAL, 1);
            gtk_widget_set_name(row, "news-item-row");

            GtkWidget* title = nullptr;

            if (is_http_url(item.url)) {
                title = gtk_button_new();
                gtk_widget_set_name(title, "news-item-title");

                g_object_set_data_full(
                    G_OBJECT(title),
                    "news-url",
                    g_strdup(item.url.c_str()),
                    g_free);

                g_signal_connect(
                    title,
                    "clicked",
                    G_CALLBACK(on_news_link_clicked),
                    nullptr);

                GtkWidget* title_label =
                    gtk_label_new(item.title.c_str());

                gtk_label_set_xalign(GTK_LABEL(title_label), 0.0f);
                gtk_label_set_line_wrap(GTK_LABEL(title_label), TRUE);
                gtk_label_set_max_width_chars(
                    GTK_LABEL(title_label), 30);
                gtk_widget_set_halign(title_label, GTK_ALIGN_START);

                gtk_container_add(
                    GTK_CONTAINER(title), title_label);
            } else {
                title = gtk_label_new(item.title.c_str());
                gtk_widget_set_name(title, "news-item-title");
                gtk_label_set_xalign(GTK_LABEL(title), 0.0f);
                gtk_label_set_line_wrap(GTK_LABEL(title), TRUE);
                gtk_label_set_max_width_chars(
                    GTK_LABEL(title), 30);
                gtk_widget_set_halign(title, GTK_ALIGN_START);
            }

            GtkWidget* time = gtk_label_new(item.time.c_str());
            gtk_widget_set_name(time, "news-item-time");
            gtk_label_set_xalign(GTK_LABEL(time), 0.0f);
            gtk_widget_set_halign(time, GTK_ALIGN_START);

            gtk_box_pack_start(GTK_BOX(row), title, FALSE, FALSE, 0);
            gtk_box_pack_start(GTK_BOX(row), time, FALSE, FALSE, 0);
            gtk_box_pack_start(
                GTK_BOX(list_box), row, FALSE, FALSE, 0);
        }
    }

    gtk_widget_show_all(list_box);
}

void NewsWidget::free_resources()
{
    if (state)
        state->widget = nullptr;

    if (container) {
        gtk_widget_destroy(container);
        container = nullptr;
    }

    list_box = nullptr;
    refresh_button = nullptr;
    lbl_title = nullptr;
    more_button = nullptr;

    delete this;
}
