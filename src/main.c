#include <gtk/gtk.h>
#include <webkit2/webkit2.h>

typedef struct {
    char *theme;
    int width;
    int height;
    char *last_url;
} AppConfig;

static AppConfig config = {NULL, 1024, 768, NULL};

static char *get_config_path() {
    return g_build_filename(g_get_user_config_dir(), "leaf-class", "config.ini", NULL);
}

typedef struct {
    GtkWidget *overlay;
    GtkWidget *card;
    GtkWidget *ticker;
    GtkWidget *filename_label;
    GtkWidget *status_label;
    GtkWidget *progress_bar;
    GtkWidget *speed_label;
    GtkWidget *time_label;
    GtkWidget *ticker_button;
    GtkWidget *cancel_button;
    GtkWidget *dismiss_button;
    WebKitDownload *current_download;
    guint64 start_time;
    guint64 last_update_time;
    guint64 last_bytes;
} DownloadWidgets;

// ... helpers ...
static char *format_size(guint64 bytes) {
    const char *units[] = {"B", "KB", "MB", "GB", "TB"};
    int i = 0;
    double size = bytes;
    while (size >= 1024 && i < 4) {
        size /= 1024;
        i++;
    }
    return g_strdup_printf("%.1f %s", size, units[i]);
}

static char *format_time(guint64 seconds) {
    if (seconds < 60) return g_strdup_printf("%lds", seconds);
    if (seconds < 3600) return g_strdup_printf("%ldm %lds", seconds / 60, seconds % 60);
    return g_strdup_printf("%ldh %ldm", seconds / 3600, (seconds % 3600) / 60);
}

static void save_config() {
    char *config_path = get_config_path();
    char *config_dir = g_path_get_dirname(config_path);
    
    if (g_mkdir_with_parents(config_dir, 0700) == 0) {
        GKeyFile *key_file = g_key_file_new();
        g_key_file_set_string(key_file, "General", "Theme", config.theme ? config.theme : "light");
        g_key_file_set_integer(key_file, "General", "Width", config.width);
        g_key_file_set_integer(key_file, "General", "Height", config.height);
        g_key_file_set_string(key_file, "General", "LastURL", config.last_url ? config.last_url : "https://classroom.google.com/");
        
        gsize length;
        char *data = g_key_file_to_data(key_file, &length, NULL);
        g_file_set_contents(config_path, data, length, NULL);
        
        g_free(data);
        g_key_file_free(key_file);
    }
    
    g_free(config_dir);
    g_free(config_path);
}

static void load_config() {
    char *config_path = get_config_path();
    GKeyFile *key_file = g_key_file_new();
    
    if (g_key_file_load_from_file(key_file, config_path, G_KEY_FILE_NONE, NULL)) {
        if (config.theme) g_free(config.theme);
        config.theme = g_key_file_get_string(key_file, "General", "Theme", NULL);
        
        if (g_key_file_has_key(key_file, "General", "Width", NULL))
            config.width = g_key_file_get_integer(key_file, "General", "Width", NULL);
            
        if (g_key_file_has_key(key_file, "General", "Height", NULL))
            config.height = g_key_file_get_integer(key_file, "General", "Height", NULL);
            
        if (config.last_url) g_free(config.last_url);
        config.last_url = g_key_file_get_string(key_file, "General", "LastURL", NULL);
    }
    
    if (!config.theme) config.theme = g_strdup("light");
    if (!config.last_url) config.last_url = g_strdup("https://classroom.google.com/");
    
    g_key_file_free(key_file);
    g_free(config_path);
}

static void go_home(GtkWidget *widget, WebKitWebView *webview) {
    webkit_web_view_load_uri(webview, "https://classroom.google.com/");
}

static void copy_url(GtkWidget *widget, WebKitWebView *web_view) {
    const gchar *uri = webkit_web_view_get_uri(web_view);
    GtkClipboard *clipboard = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
    gtk_clipboard_set_text(clipboard, uri, -1);
}

static void set_theme(const char *theme_name, WebKitWebView *webview) {
    GtkCssProvider *provider = gtk_css_provider_new();
    GdkDisplay *display = gdk_display_get_default();
    GdkScreen *screen = gdk_display_get_default_screen(display);
    
    #define LEAF_CLASS_DATA_DIR "/usr/share/leaf-class"
    char *css_path = g_build_filename(LEAF_CLASS_DATA_DIR, "css", theme_name, NULL);
    gtk_css_provider_load_from_path(provider, css_path, NULL);
    gtk_style_context_add_provider_for_screen(screen, GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_USER);
    
    g_object_unref(provider);
    g_free(css_path);

    // WebView Theme Adaptation
    GdkRGBA color;
    if (g_strcmp0(theme_name, "transparent.css") == 0) {
        gdk_rgba_parse(&color, "rgba(0,0,0,0)");
    } else if (g_strcmp0(theme_name, "dark.css") == 0) {
        gdk_rgba_parse(&color, "#242424");
    } else {
        gdk_rgba_parse(&color, "#ffffff");
    }
    webkit_web_view_set_background_color(webview, &color);
}

static void enable_dark_mode(WebKitWebView *webview) {
    const char *script = "DarkReader.enable({brightness: 100, contrast: 100, sepia: 0});";
    webkit_web_view_evaluate_javascript(webview, script, -1, NULL, NULL, NULL, NULL, NULL);
}

static void disable_dark_mode(WebKitWebView *webview) {
    const char *script = "DarkReader.disable();";
    webkit_web_view_evaluate_javascript(webview, script, -1, NULL, NULL, NULL, NULL, NULL);
}

static void on_theme_changed(GSimpleAction *action, GVariant *parameter, gpointer user_data) {
    const char *theme = g_variant_get_string(parameter, NULL);
    WebKitWebView *webview = WEBKIT_WEB_VIEW(user_data);

    // Update state to reflect selection
    g_simple_action_set_state(action, parameter);

    if (config.theme) g_free(config.theme);
    config.theme = g_strdup(theme);
    
    char *css_file = g_strdup_printf("%s.css", theme);
    set_theme(css_file, webview);
    g_free(css_file);
    
    if (g_strcmp0(theme, "dark") == 0) {
        enable_dark_mode(webview);
    } else {
        disable_dark_mode(webview);
    }
    
    save_config();
}

static gboolean on_window_delete(GtkWidget *widget, GdkEvent *event, gpointer user_data) {
    WebKitWebView *webview = WEBKIT_WEB_VIEW(user_data);
    
    gtk_window_get_size(GTK_WINDOW(widget), &config.width, &config.height);
    
    if (config.last_url) g_free(config.last_url);
    config.last_url = g_strdup(webkit_web_view_get_uri(webview));
    
    save_config();
    
    return FALSE; // Propagate event to destroy window
}

// Correct approach for splash timeout
static gboolean close_splash_and_show_main(gpointer user_data) {
    GtkWidget **windows = (GtkWidget **)user_data;
    GtkWidget *splash = windows[0];
    GtkWidget *main_window = windows[1];
    
    gtk_widget_destroy(splash);
    gtk_widget_show_all(main_window);
    
    g_free(windows);
    return FALSE;
}

static void on_download_dismiss(GtkButton *button, DownloadWidgets *widgets) {
    gtk_widget_hide(widgets->card);
    gtk_widget_hide(widgets->ticker);
}

static void on_download_cancel(GtkButton *button, DownloadWidgets *widgets) {
    if (widgets->current_download) {
        webkit_download_cancel(widgets->current_download);
    }
    gtk_widget_hide(widgets->card);
    gtk_widget_hide(widgets->ticker);
}

static void on_download_hide(GtkButton *button, DownloadWidgets *widgets) {
    gtk_widget_hide(widgets->card);
    gtk_widget_show(widgets->ticker);
}

static void on_download_restore(GtkButton *button, DownloadWidgets *widgets) {
    gtk_widget_hide(widgets->ticker);
    gtk_widget_show(widgets->card);
}

static void on_download_failed(WebKitDownload *download, GError *error, DownloadWidgets *widgets) {
    if (g_error_matches(error, WEBKIT_DOWNLOAD_ERROR, WEBKIT_DOWNLOAD_ERROR_CANCELLED_BY_USER)) {
        // If cancelled by user, just hide everything
        gtk_widget_hide(widgets->card);
        gtk_widget_hide(widgets->ticker);
        return;
    }
    gtk_label_set_text(GTK_LABEL(widgets->status_label), "Failed");
    gtk_widget_hide(widgets->ticker);
    gtk_widget_show(widgets->card);
    
    gtk_widget_hide(widgets->cancel_button);
    gtk_widget_show(widgets->dismiss_button);
}

static void on_download_finished(WebKitDownload *download, DownloadWidgets *widgets) {
    gtk_label_set_text(GTK_LABEL(widgets->status_label), "Finished");
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(widgets->progress_bar), 1.0);
    gtk_progress_bar_set_text(GTK_PROGRESS_BAR(widgets->progress_bar), "100%");
    gtk_widget_hide(widgets->ticker);
    gtk_widget_show(widgets->card);
    
    gtk_widget_hide(widgets->cancel_button);
    gtk_widget_show(widgets->dismiss_button);
}

static void on_download_progress(WebKitDownload *download, GParamSpec *pspec, DownloadWidgets *widgets) {
    double progress = webkit_download_get_estimated_progress(download);
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(widgets->progress_bar), progress);
    char *progress_text = g_strdup_printf("%.0f%%", progress * 100);
    gtk_progress_bar_set_text(GTK_PROGRESS_BAR(widgets->progress_bar), progress_text);
    g_free(progress_text);

    guint64 current_bytes = webkit_download_get_received_data_length(download);
    guint64 total_bytes = 0; // WebKitDownload doesn't expose total size directly in 4.1 easily via getter, but response does.
    WebKitURIResponse *response = webkit_download_get_response(download);
    if (response) total_bytes = webkit_uri_response_get_content_length(response);

    guint64 now = g_get_monotonic_time();
    if (now - widgets->last_update_time > 1000000) { // Update speed every second
        double speed = (double)(current_bytes - widgets->last_bytes) / ((now - widgets->last_update_time) / 1000000.0);
        char *speed_str = format_size((guint64)speed);
        char *speed_label_text = g_strdup_printf("%s/s", speed_str);
        gtk_label_set_text(GTK_LABEL(widgets->speed_label), speed_label_text);
        g_free(speed_str);
        g_free(speed_label_text);

        if (speed > 0 && total_bytes > current_bytes) {
            guint64 remaining = (total_bytes - current_bytes) / speed;
            char *time_str = format_time(remaining);
            gtk_label_set_text(GTK_LABEL(widgets->time_label), time_str);
            g_free(time_str);
        }

        widgets->last_update_time = now;
        widgets->last_bytes = current_bytes;
    }
    
    // Update ticker text
    char *ticker_text = g_strdup_printf("Downloading... %.0f%%", progress * 100);
    gtk_button_set_label(GTK_BUTTON(widgets->ticker_button), ticker_text);
    g_free(ticker_text);
}
static gboolean on_load_failed(WebKitWebView *webview, WebKitLoadEvent load_event, char *failing_uri, GError *error, gpointer user_data) {
    if (error->domain == WEBKIT_NETWORK_ERROR || error->domain == WEBKIT_POLICY_ERROR) {
        // Simple offline/error page
        const char *html = 
            "<html><body style='background-color:#242424; color:white; font-family:sans-serif; text-align:center; padding-top:50px;'>"
            "<h1>🍂 Leaf Class 🍂</h1>"
            "<h2>Unable to load page</h2>"
            "<p>Please check your internet connection.</p>"
            "<button onclick='location.reload()' style='padding:10px 20px; cursor:pointer; background:#4CAF50; border:none; color:white; font-size:16px; border-radius:4px;'>Try Again</button>"
            "</body></html>";
        webkit_web_view_load_html(webview, html, failing_uri);
        return TRUE;
    }
    return FALSE;
}

static void on_load_changed(WebKitWebView *webview, WebKitLoadEvent load_event, gpointer user_data) {
    // user_data is now a struct or array containing url_entry and spinner
    GtkWidget **widgets = (GtkWidget **)user_data;
    GtkEntry *url_entry = GTK_ENTRY(widgets[0]);
    GtkSpinner *spinner = GTK_SPINNER(widgets[1]);
    
    if (load_event == WEBKIT_LOAD_STARTED) {
        gtk_spinner_start(spinner);
    } else if (load_event == WEBKIT_LOAD_COMMITTED) {
        const char *uri = webkit_web_view_get_uri(webview);
        if (uri) {
            gtk_entry_set_text(url_entry, uri);
        }
    } else if (load_event == WEBKIT_LOAD_FINISHED) {
        gtk_spinner_stop(spinner);
        if (g_strcmp0(config.theme, "dark") == 0) {
            enable_dark_mode(webview);
        }
    }
}

static gboolean on_web_view_close(WebKitWebView *webview, gpointer user_data) {
    // Prevent JavaScript from closing the window (e.g. window.close())
    // This is important for auth flows that try to close the popup
    return TRUE; 
}

static GtkWidget *on_web_view_create(WebKitWebView *webview, WebKitNavigationAction *navigation_action, gpointer user_data) {
    WebKitURIRequest *request = webkit_navigation_action_get_request(navigation_action);
    const char *uri = webkit_uri_request_get_uri(request);
    
    if (uri) {
        webkit_web_view_load_uri(webview, uri);
    }
    
    return NULL; // Prevent new window creation
}

static gboolean on_download_decide_destination(WebKitDownload *download, gchar *suggested_filename, DownloadWidgets *widgets) {
    GtkFileChooserNative *native;
    GtkFileChooserAction action = GTK_FILE_CHOOSER_ACTION_SAVE;
    gint res;

    native = gtk_file_chooser_native_new("Save File",
                                         GTK_WINDOW(gtk_widget_get_toplevel(widgets->overlay)),
                                         action,
                                         "_Save",
                                         "_Cancel");

    if (suggested_filename) {
        gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(native), suggested_filename);
    } else {
        gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(native), "download");
    }

    res = gtk_native_dialog_run(GTK_NATIVE_DIALOG(native));
    if (res == GTK_RESPONSE_ACCEPT) {
        char *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(native));
        char *uri = g_filename_to_uri(filename, NULL, NULL);
        webkit_download_set_destination(download, uri);
        g_free(filename);
        g_free(uri);
        
        // Update UI
        gtk_label_set_text(GTK_LABEL(widgets->filename_label), suggested_filename ? suggested_filename : "download");
        gtk_label_set_text(GTK_LABEL(widgets->status_label), "Downloading...");
        
        gtk_widget_show(widgets->cancel_button);
        gtk_widget_hide(widgets->dismiss_button);
        
        gtk_widget_show(widgets->card);
        
        widgets->start_time = g_get_monotonic_time();
        widgets->last_update_time = widgets->start_time;
        widgets->last_bytes = 0;
        
        g_object_unref(native);
        return TRUE; // Handled
    }

    g_object_unref(native);
    webkit_download_cancel(download);
    return TRUE; // Handled (cancelled)
}

static void on_download_started(WebKitWebContext *context, WebKitDownload *download, DownloadWidgets *widgets) {
    widgets->current_download = download;
    g_signal_connect(download, "decide-destination", G_CALLBACK(on_download_decide_destination), widgets);
    g_signal_connect(download, "notify::estimated-progress", G_CALLBACK(on_download_progress), widgets);
    g_signal_connect(download, "failed", G_CALLBACK(on_download_failed), widgets);
    g_signal_connect(download, "finished", G_CALLBACK(on_download_finished), widgets);
}

static void on_reload(GSimpleAction *action, GVariant *parameter, gpointer user_data) {
    webkit_web_view_reload(WEBKIT_WEB_VIEW(user_data));
}

static void on_focus_url(GSimpleAction *action, GVariant *parameter, gpointer user_data) {
    gtk_widget_grab_focus(GTK_WIDGET(user_data));
}

static void create_modal_window(GtkWindow *parent, const char *title, GtkWidget *content) {
    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_transient_for(GTK_WINDOW(window), parent);
    gtk_window_set_modal(GTK_WINDOW(window), TRUE);
    gtk_window_set_default_size(GTK_WINDOW(window), 400, 300);
    
    GtkWidget *header = gtk_header_bar_new();
    gtk_header_bar_set_title(GTK_HEADER_BAR(header), title);
    gtk_window_set_titlebar(GTK_WINDOW(window), header);
    
    GtkWidget *close_button = gtk_button_new_from_icon_name("window-close-symbolic", GTK_ICON_SIZE_BUTTON);
    g_signal_connect_swapped(close_button, "clicked", G_CALLBACK(gtk_widget_destroy), window);
    gtk_header_bar_pack_start(GTK_HEADER_BAR(header), close_button);
    
    gtk_container_add(GTK_CONTAINER(window), content);
    gtk_widget_show_all(window);
}



static void on_show_shortcuts(GSimpleAction *action, GVariant *parameter, gpointer user_data) {
    GtkWindow *parent = GTK_WINDOW(user_data);
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    g_object_set(box, "margin", 20, NULL);
    
    const char *shortcuts[] = {
        "Ctrl+R: Reload Page",
        "Ctrl+L: Focus URL Bar",
        "Alt+Left: Go Back",
        "Alt+Right: Go Forward",
        NULL
    };
    
    for (int i = 0; shortcuts[i] != NULL; i++) {
        GtkWidget *label = gtk_label_new(shortcuts[i]);
        gtk_label_set_xalign(GTK_LABEL(label), 0);
        gtk_box_pack_start(GTK_BOX(box), label, FALSE, FALSE, 0);
    }
    
    create_modal_window(parent, "Keyboard Shortcuts", box);
}

static void on_show_about(GSimpleAction *action, GVariant *parameter, gpointer user_data) {
    GtkWindow *parent = GTK_WINDOW(user_data);
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    g_object_set(box, "margin", 20, NULL);
    
    const char *info[] = {
        "<b>🍂 Leaf Class 🍂</b>",
        "A minimal Google Classroom client, written in C and built with GTK and WebKit.",
        "",
        "<b>Developer:</b> Hasan Imroz",
        "<b>GitHub:</b> <a href=\"https://github.com/hasan-psl\">hasan-psl</a>",
        "<b>Email:</b> hasanimroz.personal@gmail.com",
        "<b>Project:</b> <a href=\"https://github.com/hasan-psl/Leaf-Class\">🍂 Leaf Class 🍂</a>",
        NULL
    };
    
    for (int i = 0; info[i] != NULL; i++) {
        GtkWidget *label = gtk_label_new(info[i]);
        gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
        gtk_label_set_xalign(GTK_LABEL(label), 0);
        gtk_box_pack_start(GTK_BOX(box), label, FALSE, FALSE, 0);
    }
    
    create_modal_window(parent, "About", box);
}

static void activate(GtkApplication *app, gpointer user_data) {
    load_config();

    // --- Splash Screen ---
    GtkWidget *splash = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_decorated(GTK_WINDOW(splash), FALSE);
    gtk_window_set_position(GTK_WINDOW(splash), GTK_WIN_POS_CENTER);
    gtk_window_set_default_size(GTK_WINDOW(splash), 300, 200);
    
    GtkWidget *splash_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(splash), splash_box);
    
    // Center content
    GtkWidget *splash_label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(splash_label), "<span size='xx-large' weight='bold'>🍂 Leaf Class 🍂</span>");
    gtk_widget_set_valign(splash_label, GTK_ALIGN_CENTER);
    gtk_widget_set_halign(splash_label, GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(splash_box), splash_label, TRUE, TRUE, 0);
    
    gtk_widget_show_all(splash);
    // ---------------------

    GtkWidget *window;
    GtkWidget *webview;

    window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), "🍂 Leaf Class 🍂");
    gtk_window_set_default_size(GTK_WINDOW(window), config.width, config.height);

    char *data_dir = g_build_filename(g_get_user_data_dir(), "leaf-class", NULL);
    char *cache_dir = g_build_filename(g_get_user_cache_dir(), "leaf-class", NULL);

    // Ensure directories exist for persistence
    g_mkdir_with_parents(data_dir, 0700);
    g_mkdir_with_parents(cache_dir, 0700);

    WebKitWebsiteDataManager *manager = webkit_website_data_manager_new(
        "base-data-directory", data_dir,
        "base-cache-directory", cache_dir,
        NULL);

    WebKitWebContext *context = webkit_web_context_new_with_website_data_manager(manager);
    
    // Inject DarkReader
    #define LEAF_CLASS_DATA_DIR "/usr/share/leaf-class"

    char *darkreader_path = g_build_filename(LEAF_CLASS_DATA_DIR, "darkreader.js", NULL);
    char *darkreader_content = NULL;
    if (g_file_get_contents(darkreader_path, &darkreader_content, NULL, NULL)) {
        WebKitUserContentManager *content_manager = webkit_user_content_manager_new();
        WebKitUserScript *script = webkit_user_script_new(
            darkreader_content, 
            WEBKIT_USER_CONTENT_INJECT_TOP_FRAME, 
            WEBKIT_USER_SCRIPT_INJECT_AT_DOCUMENT_END, 
            NULL, NULL);
        webkit_user_content_manager_add_script(content_manager, script);
        webkit_user_script_unref(script);
        g_free(darkreader_content);
        
        webview = g_object_new(WEBKIT_TYPE_WEB_VIEW,
            "web-context", context,
            "user-content-manager", content_manager,
            NULL);
            
        g_object_unref(content_manager);
    } else {
        g_warning("Could not load darkreader.js from %s", darkreader_path);
        webview = webkit_web_view_new_with_context(context);
    }
    g_free(darkreader_path);
    
    // Cookie manager configuration
    WebKitCookieManager *cookie_manager = webkit_web_context_get_cookie_manager(context);
    char *cookie_file = g_build_filename(data_dir, "cookies.sqlite", NULL);
    webkit_cookie_manager_set_persistent_storage(cookie_manager, cookie_file, WEBKIT_COOKIE_PERSISTENT_STORAGE_SQLITE);
    webkit_cookie_manager_set_accept_policy(cookie_manager, WEBKIT_COOKIE_POLICY_ACCEPT_ALWAYS);
    g_free(cookie_file);
    
    WebKitSettings *settings = webkit_web_view_get_settings(WEBKIT_WEB_VIEW(webview));
    webkit_settings_set_enable_developer_extras(settings, TRUE);
    webkit_settings_set_enable_smooth_scrolling(settings, TRUE);
    
    // Create Header Bar
    GtkWidget *header_bar = gtk_header_bar_new();
    gtk_header_bar_set_show_close_button(GTK_HEADER_BAR(header_bar), TRUE);
    gtk_header_bar_set_title(GTK_HEADER_BAR(header_bar), "Leaf Class");
    gtk_window_set_titlebar(GTK_WINDOW(window), header_bar);

    // Back Button
    GtkWidget *back_button = gtk_button_new_from_icon_name("go-previous-symbolic", GTK_ICON_SIZE_BUTTON);
    g_signal_connect_swapped(back_button, "clicked", G_CALLBACK(webkit_web_view_go_back), webview);
    gtk_header_bar_pack_start(GTK_HEADER_BAR(header_bar), back_button);

    // Forward Button
    GtkWidget *forward_button = gtk_button_new_from_icon_name("go-next-symbolic", GTK_ICON_SIZE_BUTTON);
    g_signal_connect_swapped(forward_button, "clicked", G_CALLBACK(webkit_web_view_go_forward), webview);
    gtk_header_bar_pack_start(GTK_HEADER_BAR(header_bar), forward_button);

    // Home Button
    GtkWidget *home_button = gtk_button_new_from_icon_name("go-home-symbolic", GTK_ICON_SIZE_BUTTON);
    g_signal_connect(home_button, "clicked", G_CALLBACK(go_home), webview);
    gtk_header_bar_pack_end(GTK_HEADER_BAR(header_bar), home_button);

    // URL Entry
    GtkWidget *url_entry = gtk_entry_new();
    gtk_editable_set_editable(GTK_EDITABLE(url_entry), FALSE);
    gtk_widget_set_size_request(url_entry, 400, -1); // Set a reasonable width
    gtk_header_bar_set_custom_title(GTK_HEADER_BAR(header_bar), url_entry);
    
    // Spinner
    GtkWidget *spinner = gtk_spinner_new();
    gtk_header_bar_pack_start(GTK_HEADER_BAR(header_bar), spinner);

    // Connect load-changed to update entry, spinner and handle dark mode
    // We need to pass both url_entry and spinner, so we use an array of widgets
    // Note: This array must persist or be freed. Since it's main window, we can just malloc it and let it leak (OS cleans up) or attach it to the window.
    // For simplicity in this C snippet, we'll use a static buffer or malloc.
    GtkWidget **load_data = g_new(GtkWidget*, 2);
    load_data[0] = url_entry;
    load_data[1] = spinner;
    
    g_signal_connect_data(webview, "load-changed", G_CALLBACK(on_load_changed), load_data, (GClosureNotify)g_free, 0);
    g_signal_connect(webview, "load-failed", G_CALLBACK(on_load_failed), NULL);
    
    // Handle popups and closing
    g_signal_connect(webview, "create", G_CALLBACK(on_web_view_create), window);
    g_signal_connect(webview, "close", G_CALLBACK(on_web_view_close), window);

    // Copy Button
    GtkWidget *copy_button = gtk_button_new_from_icon_name("edit-copy-symbolic", GTK_ICON_SIZE_BUTTON);
    g_signal_connect(copy_button, "clicked", G_CALLBACK(copy_url), webview);
    gtk_header_bar_pack_end(GTK_HEADER_BAR(header_bar), copy_button);

    // Actions and Accelerators
    GSimpleAction *act_reload = g_simple_action_new("reload", NULL);
    g_signal_connect(act_reload, "activate", G_CALLBACK(on_reload), webview);
    g_action_map_add_action(G_ACTION_MAP(app), G_ACTION(act_reload));
    const char *accels_reload[] = {"<Ctrl>r", "F5", NULL};
    gtk_application_set_accels_for_action(app, "app.reload", accels_reload);

    GSimpleAction *act_focus_url = g_simple_action_new("focus-url", NULL);
    g_signal_connect(act_focus_url, "activate", G_CALLBACK(on_focus_url), url_entry);
    g_action_map_add_action(G_ACTION_MAP(app), G_ACTION(act_focus_url));
    const char *accels_focus[] = {"<Ctrl>l", NULL};
    gtk_application_set_accels_for_action(app, "app.focus-url", accels_focus);

    GSimpleAction *act_shortcuts = g_simple_action_new("shortcuts", NULL);
    g_signal_connect(act_shortcuts, "activate", G_CALLBACK(on_show_shortcuts), window);
    g_action_map_add_action(G_ACTION_MAP(app), G_ACTION(act_shortcuts));

    GSimpleAction *act_about = g_simple_action_new("about", NULL);
    g_signal_connect(act_about, "activate", G_CALLBACK(on_show_about), window);
    g_action_map_add_action(G_ACTION_MAP(app), G_ACTION(act_about));

    // Theme Menu Action
    GSimpleAction *act_theme = g_simple_action_new_stateful("theme", G_VARIANT_TYPE_STRING, g_variant_new_string(config.theme));
    g_signal_connect(act_theme, "activate", G_CALLBACK(on_theme_changed), webview);
    g_action_map_add_action(G_ACTION_MAP(app), G_ACTION(act_theme));

    // Menu Structure
    GMenu *menu = g_menu_new();
    
    GMenu *theme_menu = g_menu_new();
    g_menu_append(theme_menu, "Light", "app.theme::light");
    g_menu_append(theme_menu, "Dark", "app.theme::dark");
    g_menu_append_submenu(menu, "Themes", G_MENU_MODEL(theme_menu));
    
    g_menu_append(menu, "Keyboard Shortcuts", "app.shortcuts");
    g_menu_append(menu, "About", "app.about");

    GtkWidget *menu_button = gtk_menu_button_new();
    GtkWidget *menu_icon = gtk_image_new_from_icon_name("open-menu-symbolic", GTK_ICON_SIZE_BUTTON);
    gtk_container_add(GTK_CONTAINER(menu_button), menu_icon);
    gtk_menu_button_set_menu_model(GTK_MENU_BUTTON(menu_button), G_MENU_MODEL(menu));
    gtk_menu_button_set_use_popover(GTK_MENU_BUTTON(menu_button), FALSE); // Use traditional menu for submenus
    gtk_header_bar_pack_end(GTK_HEADER_BAR(header_bar), menu_button);
    
    // Set initial theme
    char *css_file = g_strdup_printf("%s.css", config.theme);
    set_theme(css_file, WEBKIT_WEB_VIEW(webview));
    g_free(css_file);
    
    // Download UI Setup
    DownloadWidgets *dl_widgets = g_new0(DownloadWidgets, 1);
    
    GtkWidget *overlay = gtk_overlay_new();
    dl_widgets->overlay = overlay;
    gtk_container_add(GTK_CONTAINER(overlay), webview);
    
    // Download Card
    GtkWidget *card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_style_context_add_class(gtk_widget_get_style_context(card), "download-card");
    gtk_widget_set_halign(card, GTK_ALIGN_START);
    gtk_widget_set_valign(card, GTK_ALIGN_END);
    gtk_widget_set_margin_start(card, 20);
    gtk_widget_set_margin_bottom(card, 20);
    gtk_widget_set_size_request(card, 300, -1);
    
    // Card Header (Filename + Hide Button)
    GtkWidget *card_header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    GtkWidget *filename_label = gtk_label_new("filename.ext");
    gtk_label_set_ellipsize(GTK_LABEL(filename_label), PANGO_ELLIPSIZE_MIDDLE);
    gtk_box_pack_start(GTK_BOX(card_header), filename_label, TRUE, TRUE, 0);
    dl_widgets->filename_label = filename_label;
    
    GtkWidget *hide_button = gtk_button_new_from_icon_name("window-minimize-symbolic", GTK_ICON_SIZE_BUTTON);
    gtk_widget_set_tooltip_text(hide_button, "Hide to Ticker");
    g_signal_connect(hide_button, "clicked", G_CALLBACK(on_download_hide), dl_widgets);
    gtk_box_pack_end(GTK_BOX(card_header), hide_button, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(card), card_header, FALSE, FALSE, 0);
    
    // Status & Progress
    GtkWidget *status_label = gtk_label_new("Downloading...");
    gtk_widget_set_halign(status_label, GTK_ALIGN_START);
    dl_widgets->status_label = status_label;
    gtk_box_pack_start(GTK_BOX(card), status_label, FALSE, FALSE, 0);
    
    GtkWidget *progress_bar = gtk_progress_bar_new();
    gtk_progress_bar_set_show_text(GTK_PROGRESS_BAR(progress_bar), TRUE);
    dl_widgets->progress_bar = progress_bar;
    gtk_box_pack_start(GTK_BOX(card), progress_bar, FALSE, FALSE, 0);
    
    // Info (Speed, Time)
    GtkWidget *info_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    GtkWidget *speed_label = gtk_label_new("-");
    dl_widgets->speed_label = speed_label;
    gtk_box_pack_start(GTK_BOX(info_box), speed_label, TRUE, TRUE, 0);
    
    GtkWidget *time_label = gtk_label_new("-");
    dl_widgets->time_label = time_label;
    gtk_box_pack_start(GTK_BOX(info_box), time_label, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(card), info_box, FALSE, FALSE, 0);
    
    // Buttons (Pause/Cancel/Dismiss)
    GtkWidget *btn_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    
    GtkWidget *cancel_button = gtk_button_new_with_label("Cancel");
    g_signal_connect(cancel_button, "clicked", G_CALLBACK(on_download_cancel), dl_widgets);
    gtk_box_pack_end(GTK_BOX(btn_box), cancel_button, FALSE, FALSE, 0);
    dl_widgets->cancel_button = cancel_button;
    
    GtkWidget *dismiss_button = gtk_button_new_with_label("Dismiss");
    g_signal_connect(dismiss_button, "clicked", G_CALLBACK(on_download_dismiss), dl_widgets);
    gtk_box_pack_end(GTK_BOX(btn_box), dismiss_button, FALSE, FALSE, 0);
    dl_widgets->dismiss_button = dismiss_button;
    
    gtk_box_pack_start(GTK_BOX(card), btn_box, FALSE, FALSE, 0);
    
    dl_widgets->card = card;
    gtk_overlay_add_overlay(GTK_OVERLAY(overlay), card);
    gtk_widget_set_no_show_all(card, FALSE); // Prevent show_all from showing this
    gtk_widget_hide(card);
    
    // Ticker
    GtkWidget *ticker = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    // Class moved to button for better styling
    gtk_widget_set_halign(ticker, GTK_ALIGN_START);
    gtk_widget_set_valign(ticker, GTK_ALIGN_END);
    gtk_widget_set_margin_start(ticker, 20);
    gtk_widget_set_margin_bottom(ticker, 20);
    
    GtkWidget *ticker_button = gtk_button_new_with_label("Downloads Manager");
    gtk_style_context_add_class(gtk_widget_get_style_context(ticker_button), "download-ticker");
    g_signal_connect(ticker_button, "clicked", G_CALLBACK(on_download_restore), dl_widgets);
    gtk_box_pack_start(GTK_BOX(ticker), ticker_button, TRUE, TRUE, 0);
    dl_widgets->ticker_button = ticker_button;
    
    dl_widgets->ticker = ticker;
    gtk_overlay_add_overlay(GTK_OVERLAY(overlay), ticker);
    gtk_widget_set_no_show_all(ticker, FALSE); // Prevent show_all from showing this
    gtk_widget_hide(ticker);
    
    // Connect download-started
    g_signal_connect(context, "download-started", G_CALLBACK(on_download_started), dl_widgets);
    
    gtk_container_add(GTK_CONTAINER(window), overlay);

    webkit_web_view_load_uri(WEBKIT_WEB_VIEW(webview), config.last_url);

    g_signal_connect(window, "delete-event", G_CALLBACK(on_window_delete), webview);

    // Schedule splash close and main window show
    GtkWidget **windows = g_new(GtkWidget*, 2);
    windows[0] = splash;
    windows[1] = window;
    g_timeout_add(1500, close_splash_and_show_main, windows);

    g_free(data_dir);
    g_free(cache_dir);
    g_object_unref(manager);
    g_object_unref(context);
}

int main(int argc, char **argv) {
    GtkApplication *app;
    int status;

    app = gtk_application_new("com.example.LeafClass", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
    status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);

    return status;
}
