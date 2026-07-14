#include <SDL3/SDL.h>

#include <stdio.h>   /* fprintf  */
#include <stdbool.h> /* bool     */
#include <stdlib.h>  /* size_t   */
#include <stdint.h>  /* uint64_t */

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

        SDL_FPoint last_point;

        PointPool pool;

        Stroke current_stroke;

        StrokeList stroke_list;

        SDL_Color bg;        // canvas background color
        SDL_Color pen_color; // draw tool color
} App;

bool App_init(App *app)
{
        app->running = true;
        app->drawing = false;
        app->needs_redraw = false;

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
                                app->current_stroke.start = app->pool.count;

                                SDL_FPoint p = { event->button.x, event->button.y };
                                point_pool_add(&app->pool, p);

                                app->last_point = p;
                                app->drawing = true;
                                app->needs_redraw = true;
                        } else if (event->button.button == SDL_BUTTON_X1) {
                        } else if (event->button.button == SDL_BUTTON_X2) {
                        }
                        break;
                }

                case SDL_EVENT_MOUSE_BUTTON_UP: {
                        if (event->button.button == SDL_BUTTON_LEFT) {
                                app->current_stroke.length = 
                                        app->pool.count - app->current_stroke.start;

                                stroke_list_add(&app->stroke_list, app->current_stroke);

                                app->drawing = false;
                        }
                        break;
                }

                case SDL_EVENT_MOUSE_MOTION: {
                        if (app->drawing) {
                                SDL_FPoint p = { event->motion.x, event->motion.y };
                                point_pool_add(&app->pool, p);

                                app->last_point = p;
                                app->needs_redraw = true;
                        }
                        break;
                }
        }
}

/* simple render */
void render(SDL_Renderer *renderer, App *app)
{
        SDL_SetRenderDrawColor(renderer, 
                               app->bg.r, 
                               app->bg.g, 
                               app->bg.b, 
                               app->bg.a);

        SDL_RenderClear(renderer);

        SDL_SetRenderDrawColor(renderer, 
                               app->pen_color.r,
                               app->pen_color.g,
                               app->pen_color.b,
                               app->pen_color.a);

        /* draw all the finished strokes */
        for (int i = 0; i < app->stroke_list.count; i++) {
                Stroke stroke = app->stroke_list.data[i];
                int end = stroke.start + stroke.length;
                for (int j = stroke.start; j < end; j++) {
                        SDL_FRect rect;
                        rect.x = app->pool.points[j].x;
                        rect.y = app->pool.points[j].y;
                        rect.w = 10.0;
                        rect.h = 10.0;
                        // know that rect will be drawn off side. fix later.

                        SDL_RenderFillRect(renderer, &rect);
                }
        }

        /* draw ongoing stroke */
        for (int i = app->current_stroke.start; i < app->pool.count; i++) 
        {
                SDL_FRect rect;
                rect.x = app->pool.points[i].x;
                rect.y = app->pool.points[i].y;
                rect.w = 10.0;
                rect.h = 10.0;
                // know that rect will be drawn off side. fix later.

                SDL_RenderFillRect(renderer, &rect);
        }
        SDL_RenderPresent(renderer);
}

int main()
{
        if (!SDL_Init(SDL_INIT_VIDEO)) {
                fprintf(stderr, "%s\n", SDL_GetError());
                return 1;
        }

        SDL_Window *window = SDL_CreateWindow("wb", 1280, 720, 0);
        if (window == NULL) {
                fprintf(stderr, "%s\n", SDL_GetError());
                return 1;
        }

        SDL_Renderer *renderer = SDL_CreateRenderer(window, NULL);
        if (renderer == NULL) {
                fprintf(stderr, "%s\n", SDL_GetError());
                return 1;
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
                render(renderer, &app);
                /*
                Sint32 wait_ms = app.needs_redraw ? 0 : -1;

                if (SDL_WaitEventTimeout(&event, wait_ms)) {
                        handle_event(&event, &app);
                }

                if (app.needs_redraw) {
                        render(renderer, &app);
                        app.needs_redraw = false;
                        // last_render = now;
                }
                */
        }

        return 0;
}

