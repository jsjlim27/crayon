#include <SDL3/SDL.h>

#include <stdio.h>   /* fprintf */
#include <stdbool.h> /* bool */
#include <stdlib.h>  /* malloc, realloc, free */

/*
typedef enum {
        DRAW_RECT,
        DRAW_CIRCLE
        //DRAW_TEXTURE
} DrawKind;

typedef struct Drawable {
        DrawKind kind;
        SDL_FPoint pos;
        union {
                struct { float w; float h; } rect;
                struct { float radius; } circle;
                struct { } texture;
        };
} Drawable;
*/

/* Record of every user-drawn mouse position. */
/* boop */
/* bop */
/* beep */
typedef struct PointPool  {
        SDL_FPoint *points; // lives in the heap so it can grow.
        int count;
        int cap;
} PointPool;

/*  */
typedef struct Stroke {
        int start; // index to the stroke's beginning position residing in
                   // App.pool.points.
        int length;
        float width;
        SDL_Color color;
} Stroke;

typedef struct StrokeList {
        Stroke *data;
        int count;
        int cap;
} StrokeList;

typedef struct App {
        bool running;
        bool drawing;
        bool needs_redraw;
        int redo_count;

        SDL_FPoint last_point;

        PointPool pool;

        Stroke current_stroke;

        StrokeList stroke_list;

        SDL_Color bg;        // canvas background color
        SDL_Color pen_color; // draw tool color

        float width;
} App;

bool App_init(App *app)
{
        // welp whut
        app->running = true;
        app->drawing = false;
        app->needs_redraw = true;
        app->redo_count = 0;

        app->last_point.x = 0.0;
        app->last_point.y = 0.0;

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

        app->bg.r = 0;
        app->bg.g = 0;
        app->bg.b = 0;
        app->bg.a = 255;

        /* lavender */
        app->pen_color.r = 211;
        app->pen_color.g = 211;
        app->pen_color.b = 255;
        app->pen_color.a = 255;

        app->width = 10.0f;

        return true;
}

bool point_pool_add(PointPool *pool, SDL_FPoint p)
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

/*
bool point_pool_add_segment(PointPool *pool, SDL_FPoint a, SDL_FPoint b)
{

        for (int i = ; ; i++) {
                SDL_FPoint p;
                p.x = a.x + i * (b.x - a.x);
                point_pool_add(pool, p);
        }
}
*/

bool stroke_list_add(StrokeList *list, Stroke s)
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

/* only job should be handling inputs, no rendering */
void handle_event(SDL_Event *event, App *app)
{
        switch (event->type) {
                case SDL_EVENT_QUIT:
                        app->running = false;
                        break;

                case SDL_EVENT_MOUSE_BUTTON_DOWN: {
                        if (event->button.button == SDL_BUTTON_LEFT) {
                                app->redo_count = 0;

                                app->current_stroke.start = app->pool.count;
                                app->current_stroke.width = app->width;

                                SDL_FPoint p = { event->button.x, event->button.y };
                                point_pool_add(&app->pool, p);

                                app->last_point = p;
                                app->drawing = true;
                                app->needs_redraw = true;
                        } else if (event->button.button == SDL_BUTTON_X1) {
                                /* UNDO */
                                if (!app->drawing && app->stroke_list.count > 0) {
                                        app->stroke_list.count--;
                                        Stroke last = 
                                                app->stroke_list.data[app->stroke_list.count];

                                        app->pool.count = last.start;
                                        app->redo_count++;
                                        app->needs_redraw = true;
                                }
                        } else if (event->button.button == SDL_BUTTON_X2) {
                                /* REDO */
                                if (!app->drawing && app->redo_count > 0) {
                                        Stroke s = 
                                                app->stroke_list.data[app->stroke_list.count];

                                        app->pool.count = s.start + s.length;
                                        app->stroke_list.count++;
                                        app->redo_count--;
                                        app->needs_redraw = true;
                                }
                        }
                        break;
                }

                case SDL_EVENT_MOUSE_BUTTON_UP: {
                        if (event->button.button == SDL_BUTTON_LEFT) {
                                app->current_stroke.length = 
                                        app->pool.count - app->current_stroke.start;

                                stroke_list_add(&app->stroke_list, app->current_stroke);

                                app->drawing = false;
                                app->needs_redraw = true;
                        }
                        break;
                }

                case SDL_EVENT_MOUSE_MOTION: {
                        if (app->drawing) {
                                SDL_FPoint a = app->last_point;
                                SDL_FPoint b = { event->motion.x, event->motion.y };

                                // fixed-pixel-spacing linear interpolation
                                int spacing = 2; // in pixels
                                float dist = SDL_sqrtf( (b.x - a.x) * (b.x - a.x) + (b.y - a.y) * (b.y - a.y) );
                                int n = dist / spacing;

                                for (int i = 0; i < n - 1; i++) {
                                        SDL_FPoint p;
                                        p.x = a.x + (b.x - a.x) / n * (i + 1);
                                        p.y = a.y + (b.y - a.y) / n * (i + 1);
                                        point_pool_add(&app->pool, p);
                                }
                                point_pool_add(&app->pool, b);
                                 
                                app->last_point = b;
                                app->needs_redraw = true;
                        }
                        break;
                }

                case SDL_EVENT_KEY_DOWN: //{
                        /*
                        if (event->key.scancode == SDL_SCANCODE_F && !event->key.repeat && !app->drawing) {
                                app->redo_count = 0;

                                app->current_stroke.start = app->pool.count;
                                app->current_stroke.width = app->width;

                                float x, y;
                                SDL_GetMouseState(&x, &y);
                                SDL_FPoint p = { x, y };
                                point_pool_add(&app->pool, p);

                                app->last_point = p;
                                app->drawing = true;
                                app->needs_redraw = true;
                        }
                        */
                        break;
                //}

                case SDL_EVENT_KEY_UP: //{
                        /*
                        if (event->key.scancode == SDL_SCANCODE_F) {
                        }
                        */
                        break;
                //}

                case SDL_EVENT_WINDOW_EXPOSED:
                        fprintf(stderr, "EXPOSED\n");
                        app->needs_redraw = true;
                        break;
        }
}

/* simple render */
void render(SDL_Renderer *renderer, App *app)
{
        SDL_Color c = { 60, 60, 76, 255 };
        SDL_SetRenderDrawColor(renderer, 
                               c.r,
                               c.g,
                               c.b,
                               c.a);

        SDL_RenderClear(renderer);

        SDL_SetRenderDrawColor(renderer, 
                               app->pen_color.r,
                               app->pen_color.g,
                               app->pen_color.b,
                               app->pen_color.a);

        // draw all the finished strokes
        for (int i = 0; i < app->stroke_list.count; i++) {
                Stroke s = app->stroke_list.data[i];
                int end = s.start + s.length;
                for (int j = s.start; j < end; j++) {
                        SDL_FRect rect;
                        rect.x = app->pool.points[j].x - s.width / 2;
                        rect.y = app->pool.points[j].y - s.width / 2;
                        rect.w = s.width;
                        rect.h = s.width;

                        SDL_RenderFillRect(renderer, &rect);
                }
        }

        // draw ongoing stroke
        if (app->drawing) {
        SDL_Color c = { 11, 218, 81, 255 };
        SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, c.a);
        for (int i = app->current_stroke.start; i < app->pool.count; i++) 
        {
                SDL_FRect rect;
                rect.x = app->pool.points[i].x - app->current_stroke.width / 2;
                rect.y = app->pool.points[i].y - app->current_stroke.width / 2;
                rect.w = app->current_stroke.width;
                rect.h = app->current_stroke.width;

                SDL_RenderFillRect(renderer, &rect);
        }
        }
        SDL_RenderPresent(renderer);
}

int main()
{
        if (!SDL_Init(SDL_INIT_VIDEO)) {
                fprintf(stderr, "%s\n", SDL_GetError());
                return 1;
        }

        SDL_Window *window = SDL_CreateWindow("wb", 1350, 1350, 0);
        // SDL_Window *window = SDL_CreateWindow("wb", 800, 600, 0);
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
        if (!App_init(&app)) {
                // print err
                return 1;
        }

        SDL_Event event;
        while (app.running) {
                while (SDL_PollEvent(&event)) {
                        handle_event(&event, &app);
                }
                if (app.needs_redraw) {
                        static int redraw_count = 0;
                        fprintf(stderr, "%i: re-drawing...\n", redraw_count++);
                        fprintf(stderr, "pool count: %i\n", app.pool.count);
                        fprintf(stderr, "pool cap  : %i\n", app.pool.cap);
                        render(renderer, &app);
                        app.needs_redraw = false;
                }
        }

        return 0;
}

