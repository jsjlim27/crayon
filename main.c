/*
 * TODO:
 * [] file
 *      [] save session to file
 * [] cursor
 * [] circle brush
 */
#include <SDL3/SDL.h>

#include <stdio.h>   /* fprintf */
#include <stdbool.h> /* bool */
#include <stdlib.h>  /* malloc, realloc, free */
#include <time.h>    /* for timestamping session saves  */

/* enums */
/* only circle for now.
 * typedef enum { BRUSH_RECT, BRUSH_CIRCLE, BRUSH_COUNT } BrushType;
 */
typedef enum { INPUT_NONE, INPUT_MOUSE, INPUT_KEY, INPUT_PEN } StrokeInput;
typedef enum { REGION_CANVAS, REGION_PALETTE, REGION_COUNT } UiRegion;

/* Record of every user-drawn mouse position. */
typedef struct {
        SDL_FPoint *points; // lives in the heap so it can grow.
                            // stores brush stamp positions.
        int count;
        int cap;
} PointPool;

/*  structs */
typedef struct {
        int start;       // stroke's starting position (stored in the point pool).
        int length;      // number of points that make up the stroke.
        float width;     // the stroke's width.
        SDL_Color color; // color of the stroke.
        // BrushType type;  only circle for now.
} Stroke;

typedef struct {
        Stroke *data;
        int count;
        int cap;
} StrokeList;

/* THE GOD STRUCT */
typedef struct {
        SDL_Point window_size;

        PointPool pool;
        Stroke current_stroke;
        StrokeList stroke_list;

        SDL_Window *window;
        SDL_Renderer *renderer;
        SDL_Texture *circle_texture;

        SDL_FPoint position_offset;

        bool focused;
        bool running;
        bool drawing;
        bool panning;
        bool redraw;

        int redo_count;

        SDL_FPoint last_point;
        SDL_Color bg_color;        // canvas background color

        /* Current brush settings. Copied into each Stroke at app_stroke_begin;
           changing these never affect strokes already drawn. */
        // BrushType brush_type;
        SDL_Color brush_color;
        float     brush_width;

        StrokeInput stroke_input; // for distinguishing between inputs 
                                  // (e.g., INPUT_MOUSE or INPUT_KEY).
} App;

/****************** functions *******************/
/*
 * Name: screen_to_world
 */
static SDL_FPoint screen_to_world(SDL_FPoint offset, SDL_FPoint p)
{
        return (SDL_FPoint){ p.x + offset.x, p.y + offset.y };
}

static SDL_FPoint world_to_screen(SDL_FPoint offset, SDL_FPoint p)
{
        return (SDL_FPoint){ p.x - offset.x, p.y - offset.y };
}

/*
 * Name       : dist_squared
 * Description: Calculate the square of the distance between points a and b.
 * 
 * Pre-condition  : none
 * Post-condition : Returns square distance between a and b.
 *
 */
static float dist_squared(SDL_FPoint a, SDL_FPoint b)
{
        float dx = b.x - a.x;
        float dy = b.y - a.y;

        return dx * dx + dy * dy;
}

/*
 * Name: create_surface_circle
 * Description: Returns a SDL Surface that represents a circle.
 *
 */
static SDL_Surface *create_surface_circle(SDL_Color c)
{
        const int w = 256;
        const int h = 256;

        SDL_Surface *surf = SDL_CreateSurface(w, h, SDL_PIXELFORMAT_RGBA32);
        if (surf == NULL) { return NULL; }

        SDL_Color *pixels = surf->pixels;
        SDL_FPoint center = { w / 2.0f, h / 2.0f };
        float r = w / 2.0f;

        for (int y = 0; y < h; y++) {
                for (int x = 0; x < w; x++) {
                        pixels[w * y + x].r = c.r;
                        pixels[w * y + x].g = c.g;
                        pixels[w * y + x].b = c.b;

                        SDL_FPoint p = { x, y };
                        float dist2 = dist_squared(p, center);
                        if (dist2 > r * r) {
                                pixels[w * y + x].a = 0;
                        } else {
                                pixels[w * y + x].a = (Uint8)(255.0f - dist2 / (r * r) * 255.0f);
                        }
                }
        }
        return surf;
}

static bool app_init(App *app)
{
        app->window = NULL;
        app->renderer = NULL;

        app->panning = false;
        app->position_offset.x = 0.0f;
        app->position_offset.y = 0.0f;

        app->focused = true;
        app->running = true;
        app->drawing = false;
        app->redraw = true;
        app->redo_count = 0;

        app->last_point.x = 0.0f;
        app->last_point.y = 0.0f;

        /* Assume allocation always succeeds for now. */
        app->pool.points = malloc(65536 * sizeof(SDL_FPoint));
        app->pool.count = 0;
        app->pool.cap = 65536;

        app->current_stroke.start = 0;
        app->current_stroke.length = 0;

        /* Assume allocation always succeeds for now. */
        app->stroke_list.data = malloc(16384 * sizeof(Stroke));
        app->stroke_list.count = 0;
        app->stroke_list.cap = 16384;

        /* white bg */
        /*
        app->bg_color.r = 255;
        app->bg_color.g = 255;
        app->bg_color.b = 255;
        app->bg_color.a = 255;
        */

        /* black bg */
        app->bg_color.r = 0;
        app->bg_color.g = 0;
        app->bg_color.b = 0;
        app->bg_color.a = 255;

        // app->brush_type = BRUSH_RECT;

        /* green (from krita) */
        /*
        app->brush_color.r = 0;
        app->brush_color.g = 255;
        app->brush_color.b = 41;
        app->brush_color.a = 255;
        */

        /* black */
        /*
        app->brush_color.r = 0;
        app->brush_color.g = 0;
        app->brush_color.b = 0;
        app->brush_color.a = 255;
        */

        /* white */
        app->brush_color.r = 255;
        app->brush_color.g = 255;
        app->brush_color.b = 255;
        app->brush_color.a = 255;

        /* lavender */
        /*
        app->brush_color.r = 211;
        app->brush_color.g = 211;
        app->brush_color.b = 255;
        app->brush_color.a = 255;
        */

        app->brush_width = 10.0f;

        app->stroke_input = INPUT_NONE;

        app->circle_texture = NULL;
        return true;
}

static bool point_pool_add(PointPool *pool, SDL_FPoint p)
{
        /* check if reached capacity */
        if (pool->count == pool->cap) {
                int new_cap = pool->cap * 2;
                SDL_FPoint *temp = realloc(pool->points,
                                           new_cap * sizeof(SDL_FPoint));
                if (temp == NULL) { return false; }
                pool->points = temp;
                pool->cap = new_cap;
        }
        pool->points[pool->count++] = p;
        return true;
}

static bool point_pool_add_segment(PointPool *pool, SDL_FPoint a, SDL_FPoint b, float spacing)
{
        float dx = b.x - a.x;
        float dy = b.y - a.y;
        float dist = SDL_sqrtf(dx * dx + dy * dy);
        int n = (int)(dist / spacing);
        for (int i = 1; i <= n; i++) {
                float t = (i * spacing) / dist;
                SDL_FPoint p = { a.x + dx * t, a.y + dy * t };
                if (!point_pool_add(pool, p)) {
                        return false;
                }
        }
        return point_pool_add(pool, b);
}

static bool stroke_list_add(StrokeList *list, Stroke s)
{
        /* check if reached capacity */
        if (list->count == list->cap) {
                int new_cap = list->cap * 2;
                Stroke *temp = realloc(list->data,
                                       new_cap * sizeof(Stroke));
                if (temp == NULL) { return false; }
                list->data = temp;
                list->cap = new_cap;
        }
        list->data[list->count++] = s;
        return true;
}

static void app_stroke_undo(App *app)
{
        if (!app->drawing && app->stroke_list.count > 0) {
                app->stroke_list.count--;

                const Stroke *last = 
                        &app->stroke_list.data[app->stroke_list.count];

                app->pool.count = last->start;

                app->redo_count++;
                app->redraw = true;
        }
}

static void app_stroke_redo(App *app)
{
        if (!app->drawing && app->redo_count > 0) {
                const Stroke *last = 
                        &app->stroke_list.data[app->stroke_list.count];
                app->pool.count = last->start + last->length;
                app->stroke_list.count++;
                app->redo_count--;
                app->redraw = true;
        }
}

static void app_stroke_begin(App *app, SDL_FPoint p, StrokeInput from)
{
        if (app->drawing) { return; }

        app->stroke_input = from;
        app->redo_count = 0;

        app->current_stroke.start = app->pool.count;
        // app->current_stroke.type = app->brush_type;
        app->current_stroke.color = app->brush_color;
        app->current_stroke.width = app->brush_width;

        point_pool_add(&app->pool, screen_to_world(app->position_offset, p));

        app->drawing = true;

        app->last_point = p;
        app->redraw = true;
}

static void app_stroke_extend(App *app, SDL_FPoint p)
{
        if (!app->drawing) { return; }

        point_pool_add_segment(
                &app->pool, 
                screen_to_world(app->position_offset, app->last_point),
                screen_to_world(app->position_offset, p),
                2.0f
        );

        app->last_point = p;
        app->redraw = true;
}

static void app_stroke_end(App *app, StrokeInput from)
{
        if (!app->drawing || app->stroke_input != from) {
                return;
        }

        app->current_stroke.length = 
                app->pool.count - app->current_stroke.start;

        stroke_list_add(&app->stroke_list, app->current_stroke);

        app->drawing = false;
        app->stroke_input = INPUT_NONE;
        app->redraw = true;
}

static void app_brush_resize(App *app, float delta)
{
        app->brush_width += 1.5f * delta;
        if (app->brush_width < 1.0f) {
                app->brush_width = 1.0f;
        }
        if (app->brush_width > 64.0f) {
                app->brush_width = 64.0f;
        }
}

static void app_panning_begin(App *app)
{
        // shouldnt be able to pan while drawing (would be disorienting)
        // if (app->drawing) { return; }
        
        // kinda buggy on my setup
        // SDL_SetWindowRelativeMouseMode(app->window, true);
        app->panning = true;
}

static void app_panning_extend(App *app, SDL_FPoint p)
{
        if (!app->panning) { return; }
        app->position_offset.x -= 1.25f * p.x;
        app->position_offset.y -= 1.25f * p.y;
        app->redraw = true;
}

static void app_panning_end(App *app)
{
        //SDL_SetWindowRelativeMouseMode(app->window, false);
        app->panning = false;
        app->redraw = true;
}

/*
 * for color selection feature later
static UiRegion region_at_point(SDL_FPoint p, App *app)
{
        SDL_FRect palette = { .x = 0.0f, 
                              .y = 0.0f, 
                              .w = 0.1f * app->window_size.x, 
                              .h = app->window_size.y };

        //SDL_FRect canvas = { .x = 0.0f, .y = 0.0f, .w = window_w, .h = window_h };

        if (SDL_PointInRectFloat(&p, &palette)) {
                fprintf(stderr, "p in palette\n");
                return REGION_PALETTE;
        }

        return REGION_CANVAS;
}

static void app_select_brush_color(App *app, SDL_FPoint p)
{
        if (p.y > 0.0f && p.y < app->window_size.y / 2.0f) {
                // pretty red
                app->brush_color = 
                        (SDL_Color){ .r = 0xFF, .g = 0x70, .b = 0x81, .a = 0xFF };
        } else {
                // white
                app->brush_color = 
                        (SDL_Color){ .r = 0xFF, .g = 0xFF, .b = 0xFF, .a = 0xFF };
        }
}
*/

/* only job should be handling inputs, no rendering */
static void handle_event(SDL_Event *event, App *app)
{
        switch (event->type) {
        case SDL_EVENT_QUIT:
                app->running = false;
                break;

        /* one of the mouse buttons pressed down */
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
                SDL_FPoint p = { event->button.x, event->button.y };
                switch(event->button.button) {
                case SDL_BUTTON_LEFT:
                        app_stroke_begin(app, p, INPUT_MOUSE);
                        break;
                        /*
                        switch(region_at_point(p, app)) {
                        case REGION_PALETTE:
                                app_select_brush_color(app, p);
                                break;
                        case REGION_CANVAS:
                                app_stroke_begin(app, p, INPUT_MOUSE);
                                break;
                        }
                        break;
                        */
                case SDL_BUTTON_X1:
                        app_stroke_undo(app);
                        break;
                case SDL_BUTTON_X2:
                        app_stroke_redo(app);
                        break;
                case SDL_BUTTON_MIDDLE:
                        app_panning_begin(app);
                        break;
                }
                break;

        /* mouse is moving */
        case SDL_EVENT_MOUSE_MOTION:
                app_stroke_extend(
                        app,
                        (SDL_FPoint){ event->motion.x, event->motion.y }
                );
                app_panning_extend(
                        app, 
                        (SDL_FPoint){ event->motion.xrel, event->motion.yrel }
                );
                break;

        /* one of the mouse buttons pressed up 
         * (released from being pressed down) */
        case SDL_EVENT_MOUSE_BUTTON_UP:
                switch(event->button.button) {
                case SDL_BUTTON_LEFT:
                        app_stroke_end(app, INPUT_MOUSE);
                        break;
                case SDL_BUTTON_MIDDLE:
                        app_panning_end(app);
                        break;
                }
                break;

        /* mouse wheel scrolled */
        case SDL_EVENT_MOUSE_WHEEL:
                app_brush_resize(app, event->wheel.y);
                break;

        /* one of the keyboard keys pressed */
        case SDL_EVENT_KEY_DOWN:
                switch (event->key.scancode) {
                case SDL_SCANCODE_J:
                case SDL_SCANCODE_F: {
                        if (event->key.repeat) { break; }
                        float x, y;
                        SDL_GetMouseState(&x, &y);
                        SDL_FPoint p = { x, y };
                        app_stroke_begin(app, p, INPUT_KEY);
                        break;
                }
                case SDL_SCANCODE_K:
                case SDL_SCANCODE_D:
                        app_stroke_undo(app);
                        break;
                case SDL_SCANCODE_H:
                case SDL_SCANCODE_G:
                        app_stroke_redo(app);
                        break;
                }
                break;

        case SDL_EVENT_KEY_UP:
                switch (event->key.scancode) {
                case SDL_SCANCODE_J:
                case SDL_SCANCODE_F:
                        app_stroke_end(app, INPUT_KEY);
                        break;
                }
                break;

        case SDL_EVENT_WINDOW_EXPOSED:
                app->redraw = true;
                break;

        case SDL_EVENT_WINDOW_RESIZED:
                app->window_size.x = event->window.data1;
                app->window_size.y = event->window.data2;
                break;

        case SDL_EVENT_WINDOW_FOCUS_GAINED:
                app->focused = true;
                break;

        case SDL_EVENT_WINDOW_FOCUS_LOST:
                app->focused = false;
                break;
        }
}

static void draw_stroke(SDL_Renderer     *r, 
                        SDL_Texture      *t, 
                        const Stroke     *s, 
                        const SDL_FPoint *p,
                        SDL_FPoint       offset)
{
        // tint the brush texture with stroke's color
        //SDL_SetTextureColorMod(t, s->color.r, s->color.g, s->color.b);

        int end = s->start + s->length;
        float half = s->width / 2;

        for (int i = s->start; i < end; i++) {
                SDL_FPoint pos = world_to_screen(offset, p[i]);
                SDL_FRect rect = { .x = pos.x - half,
                                   .y = pos.y - half,
                                   .w = s->width,
                                   .h = s->width };

                SDL_RenderTexture(r, t, NULL, &rect);
        }
}

/* simple render */
static void render(App *app)
{
        /* draw background */
        SDL_SetRenderDrawColor(app->renderer, 
                               app->bg_color.r,
                               app->bg_color.g,
                               app->bg_color.b,
                               app->bg_color.a);

        SDL_RenderClear(app->renderer);

        // draw all the past strokes
        for (int i = 0; i < app->stroke_list.count; i++) {
                draw_stroke(app->renderer, 
                                   app->circle_texture, 
                                   &app->stroke_list.data[i], 
                                   app->pool.points,
                                   app->position_offset);
        }

        // draw ongoing stroke
        if (app->drawing) {
                Stroke live = app->current_stroke;
                live.length = app->pool.count - live.start;
                draw_stroke(app->renderer, 
                                   app->circle_texture, 
                                   &live, 
                                   app->pool.points,
                                   app->position_offset);
        }

        SDL_RenderPresent(app->renderer);
}

int main(int argc, char *argv[])
{
        if (!SDL_Init(SDL_INIT_VIDEO)) {
                fprintf(stderr, "%s\n", SDL_GetError());
                return 1;
        }

        SDL_WindowFlags window_flags = SDL_WINDOW_RESIZABLE;
        SDL_Window *window = SDL_CreateWindow("draw", 1920, 1080, window_flags);
        if (window == NULL) {
                fprintf(stderr, "%s\n", SDL_GetError());
                return 1;
        }

        SDL_Renderer *renderer = SDL_CreateRenderer(window, NULL);
        if (renderer == NULL) {
                fprintf(stderr, "%s\n", SDL_GetError());
                return 1;
        }

        if (!SDL_SetRenderVSync(renderer, SDL_RENDERER_VSYNC_DISABLED)) {
                fprintf(stderr, "%s\n", SDL_GetError());
        }

        App app;
        if (!app_init(&app)) {
                // print err
                fprintf(stderr, "app_init failed! Exiting..\n");
                return 1;
        }

        // should probably organize this somewhere else
        SDL_Surface *surf = create_surface_circle(app.brush_color);
        SDL_Texture *circle_texture = SDL_CreateTextureFromSurface(renderer, surf);
        SDL_SetTextureScaleMode(circle_texture, SDL_SCALEMODE_LINEAR);
        SDL_SetTextureBlendMode(circle_texture, SDL_BLENDMODE_BLEND);
        SDL_DestroySurface(surf);

        app.renderer = renderer;
        app.circle_texture = circle_texture;

        SDL_GetWindowSize(window, &app.window_size.x, &app.window_size.y);
        fprintf(stderr, "window_size.x: %d, window_size.y: %d\n", app.window_size.x, app.window_size.y);

        SDL_Event event;
        SDL_SetEventEnabled(SDL_EVENT_PEN_BUTTON_DOWN, false);
        SDL_SetEventEnabled(SDL_EVENT_PEN_BUTTON_UP, false);

        while (app.running) {
                if (app.focused) {
                        while (SDL_PollEvent(&event)) {
                                handle_event(&event, &app);
                        }
                } else {
                        SDL_WaitEvent(&event);
                        handle_event(&event, &app);
                }
                if (app.redraw) {
                        /*
                        static int redraw_count = 0;
                        fprintf(stderr, "%i: re-drawing...\n", redraw_count++);
                        fprintf(stderr, "pool count: %i\n", app.pool.count);
                        fprintf(stderr, "pool cap  : %i\n", app.pool.cap);
                        fprintf(stderr, "app.current_stroke.width: %f\n", app.current_stroke.width);
                        fprintf(stderr, "pan offset: (%f, %f)\n", app.offset.x, app.offset.y);
                        */
                        render(&app);
                        app.redraw = false;
                }
        }

        return 0;
}
