#include <gtk/gtk.h>
#include <gtk-layer-shell/gtk-layer-shell.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

#define BTN_SIZE 40
#define BTN_GAP   6

static int      reg_x, reg_y, reg_w, reg_h, border_px, delay_secs;
static pid_t    target_pid;
static int      countdown_val;
static gboolean recording_phase = FALSE;

/* Button top-left corners (screen coords == window coords: window fills screen) */
static int stop_bx, stop_by;
static int cncl_bx, cncl_by;

static GtkWidget *window_widget;
static GtkWidget *drawing_area;

static void do_stop(void) {
    if (target_pid > 0) kill(target_pid, SIGUSR1);
    gtk_main_quit();
}

static void do_cancel(void) {
    if (target_pid > 0) kill(target_pid, SIGUSR2);
    gtk_main_quit();
}

static void set_input_region(gboolean buttons_only) {
    cairo_region_t *region = cairo_region_create();
    if (buttons_only) {
        cairo_rectangle_int_t r;
        r.x = stop_bx; r.y = stop_by; r.width = BTN_SIZE; r.height = BTN_SIZE;
        cairo_region_union_rectangle(region, &r);
        r.x = cncl_bx; r.y = cncl_by;
        cairo_region_union_rectangle(region, &r);
    }
    /* empty region = fully click-through */
    gtk_widget_input_shape_combine_region(window_widget, region);
    cairo_region_destroy(region);
}

static gboolean on_draw(GtkWidget *widget, cairo_t *cr, gpointer data) {
    (void)widget; (void)data;

    /* Clear to fully transparent */
    cairo_set_source_rgba(cr, 0, 0, 0, 0);
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    cairo_paint(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);

    if (!recording_phase && countdown_val > 0) {
        /* Dim recording region during countdown */
        cairo_set_source_rgba(cr, 0, 0, 0, 0.65);
        cairo_rectangle(cr, reg_x, reg_y, reg_w, reg_h);
        cairo_fill(cr);

        /* Countdown number */
        char buf[8];
        snprintf(buf, sizeof(buf), "%d", countdown_val);
        cairo_select_font_face(cr, "Sans",
                               CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
        cairo_set_font_size(cr, 72.0);
        cairo_text_extents_t ext;
        cairo_text_extents(cr, buf, &ext);
        double tx = reg_x + (reg_w - ext.width)  / 2.0 - ext.x_bearing;
        double ty = reg_y + (reg_h + ext.height) / 2.0;
        cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 1.0);
        cairo_move_to(cr, tx, ty);
        cairo_show_text(cr, buf);
    }

    /* Green border */
    cairo_set_source_rgba(cr, 0.2, 0.8, 0.2, 0.9);
    cairo_set_line_width(cr, (double)border_px);
    cairo_rectangle(cr,
                    reg_x - border_px / 2.0,
                    reg_y - border_px / 2.0,
                    reg_w + (double)border_px,
                    reg_h + (double)border_px);
    cairo_stroke(cr);

    if (recording_phase) {
        int icon_sz = 14;

        /* Stop button: red circle */
        cairo_arc(cr,
                  stop_bx + BTN_SIZE / 2.0,
                  stop_by + BTN_SIZE / 2.0,
                  BTN_SIZE / 2.0, 0.0, 2.0 * G_PI);
        cairo_set_source_rgba(cr, 0.8, 0.13, 0.13, 1.0);
        cairo_fill(cr);
        /* Stop icon: white square */
        int off = (BTN_SIZE - icon_sz) / 2;
        cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 1.0);
        cairo_rectangle(cr, stop_bx + off, stop_by + off, icon_sz, icon_sz);
        cairo_fill(cr);

        /* Cancel button: gray circle */
        cairo_arc(cr,
                  cncl_bx + BTN_SIZE / 2.0,
                  cncl_by + BTN_SIZE / 2.0,
                  BTN_SIZE / 2.0, 0.0, 2.0 * G_PI);
        cairo_set_source_rgba(cr, 0.33, 0.33, 0.33, 1.0);
        cairo_fill(cr);
        /* Cancel icon: white X */
        int pad = (BTN_SIZE - icon_sz) / 2;
        cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 1.0);
        cairo_set_line_width(cr, 3.0);
        cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
        cairo_move_to(cr, cncl_bx + pad,            cncl_by + pad);
        cairo_line_to(cr, cncl_bx + BTN_SIZE - pad, cncl_by + BTN_SIZE - pad);
        cairo_stroke(cr);
        cairo_move_to(cr, cncl_bx + BTN_SIZE - pad, cncl_by + pad);
        cairo_line_to(cr, cncl_bx + pad,            cncl_by + BTN_SIZE - pad);
        cairo_stroke(cr);
    }

    return FALSE;
}

static gboolean on_click(GtkWidget *widget, GdkEventButton *ev, gpointer data) {
    (void)widget; (void)data;
    if (!recording_phase || ev->button != 1) return FALSE;

    double r  = BTN_SIZE / 2.0;
    double mx = ev->x, my = ev->y;

    double sx = stop_bx + r, sy = stop_by + r;
    if ((mx - sx) * (mx - sx) + (my - sy) * (my - sy) <= r * r) {
        do_stop();
        return TRUE;
    }
    double cx = cncl_bx + r, cy = cncl_by + r;
    if ((mx - cx) * (mx - cx) + (my - cy) * (my - cy) <= r * r) {
        do_cancel();
        return TRUE;
    }
    return FALSE;
}

static gboolean on_tick(gpointer data) {
    (void)data;
    countdown_val--;
    if (countdown_val <= 0) {
        recording_phase = TRUE;
        set_input_region(TRUE);
        gtk_widget_queue_draw(drawing_area);
        return G_SOURCE_REMOVE;
    }
    gtk_widget_queue_draw(drawing_area);
    return G_SOURCE_CONTINUE;
}

int main(int argc, char *argv[]) {
    if (argc < 7) {
        fprintf(stderr, "Usage: %s X Y W H BORDER_PX DELAY [TARGET_PID]\n", argv[0]);
        return 1;
    }
    reg_x      = atoi(argv[1]);
    reg_y      = atoi(argv[2]);
    reg_w      = atoi(argv[3]);
    reg_h      = atoi(argv[4]);
    border_px  = atoi(argv[5]);
    delay_secs = atoi(argv[6]);
    target_pid = (argc > 7) ? (pid_t)atoi(argv[7]) : 0;

    countdown_val = delay_secs;

    /* Buttons: top-right above recording area */
    stop_bx = reg_x + reg_w - BTN_SIZE;
    stop_by = reg_y - border_px - BTN_SIZE - 4;
    cncl_bx = stop_bx - BTN_SIZE - BTN_GAP;
    cncl_by = stop_by;

    gtk_init(&argc, &argv);

    window_widget = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window_widget), "screensnipe-overlay");
    gtk_window_set_decorated(GTK_WINDOW(window_widget), FALSE);
    gtk_window_set_resizable(GTK_WINDOW(window_widget), FALSE);

    /* RGBA visual for transparency */
    GdkScreen *screen = gtk_widget_get_screen(window_widget);
    GdkVisual *visual = gdk_screen_get_rgba_visual(screen);
    if (visual) gtk_widget_set_visual(window_widget, visual);
    gtk_widget_set_app_paintable(window_widget, TRUE);

    /* gtk-layer-shell: full-screen overlay on top of everything */
    gtk_layer_init_for_window(GTK_WINDOW(window_widget));
    gtk_layer_set_layer(GTK_WINDOW(window_widget), GTK_LAYER_SHELL_LAYER_OVERLAY);
    gtk_layer_set_exclusive_zone(GTK_WINDOW(window_widget), -1);
    gtk_layer_set_anchor(GTK_WINDOW(window_widget), GTK_LAYER_SHELL_EDGE_TOP,    TRUE);
    gtk_layer_set_anchor(GTK_WINDOW(window_widget), GTK_LAYER_SHELL_EDGE_BOTTOM, TRUE);
    gtk_layer_set_anchor(GTK_WINDOW(window_widget), GTK_LAYER_SHELL_EDGE_LEFT,   TRUE);
    gtk_layer_set_anchor(GTK_WINDOW(window_widget), GTK_LAYER_SHELL_EDGE_RIGHT,  TRUE);
    gtk_layer_set_keyboard_mode(GTK_WINDOW(window_widget),
                                GTK_LAYER_SHELL_KEYBOARD_MODE_NONE);

    drawing_area = gtk_drawing_area_new();
    gtk_widget_add_events(drawing_area, GDK_BUTTON_PRESS_MASK);
    g_signal_connect(drawing_area, "draw",               G_CALLBACK(on_draw),  NULL);
    g_signal_connect(drawing_area, "button-press-event", G_CALLBACK(on_click), NULL);
    gtk_container_add(GTK_CONTAINER(window_widget), drawing_area);

    g_signal_connect(window_widget, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    gtk_widget_show_all(window_widget);

    if (delay_secs > 0) {
        set_input_region(FALSE);  /* click-through during countdown */
        g_timeout_add(1000, on_tick, NULL);
    } else {
        recording_phase = TRUE;
        set_input_region(TRUE);
    }

    gtk_main();
    return 0;
}
