#include <SDL3/SDL.h>

#include <stdbool.h>/* bool     */
#include <stdlib.h> /* size_t   */
#include <stdint.h> /* uint64_t */

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

typedef struct PointPool  {
        SDL_FPoint *points;
        int count;
        int cap;
} PointPool;

typedef struct Stroke {
        int start;
        int length;
} Stroke;

typedef struct App {
        bool running;
        bool drawing;
        bool needs_redraw;

        SDL_FPoint last_point;

        PointPool pool;

        Stroke current_stroke;

        Stroke *strokes;
        int strokes_count;
        int strokes_cap;

        SDL_Color bg;
        SDL_Color pen_color;
} App;

bool App_init(App *app)
{
        app->running = true;
        app->drawing = false;
        app->needs_redraw = false;

        app->last_point.x = 0.0;
        app->last_point.y = 0.0;

        // for now assume guaranteed success
        app->pool.points = malloc(65536 * sizeof(SDL_FPoint));
        app->pool.count = 0;
        app->pool.cap = 65536;

        app->current_stroke.start = 0;
        app->current_stroke.length = 0;

        // for now assume guaranteed success
        app->strokes = malloc(16384 * sizeof(Stroke));
        app->strokes_count = 0;
        app->strokes_cap = 16384;

        app->bg.r = 0;
        app->bg.g = 0;
        app->bg.b = 0;
        app->bg.a = 255;

        app->pen_color.r = 255;
        app->pen_color.g = 66;
        app->pen_color.b = 200;
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
                        }
                        break;
                }

                case SDL_EVENT_MOUSE_BUTTON_UP: {
                        if (app->strokes_count == app->strokes_cap) {
                                int new_cap = 2 * app->strokes_cap;
                                Stroke *temp = realloc(app->strokes, new_cap * sizeof(Stroke));
                                //if (temp == NULL) { return; }
                                // for now, just assume reallocation always succeed.
                                app->strokes = temp;
                                app->strokes_cap = new_cap;
                        }
                        app->current_stroke.length = 
                                app->pool.count - app->current_stroke.start;

                        app->strokes[app->strokes_count++] = app->current_stroke;

                        app->drawing = false;
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

void render(SDL_Renderer *renderer, App *app)
{
        SDL_SetRenderDrawColor(renderer, 
                               app->bg.r, app->bg.g, app->bg.b, app->bg.a);

        SDL_RenderClear(renderer);

        SDL_SetRenderDrawColor(renderer, 
                               app->pen_color.r, app->pen_color.g, app->pen_color.b, app->pen_color.a);

        for (int i = 0; i < app->strokes_count; i++) {
                Stroke s = app->strokes[i];
                for (int j = s.start; j < s.length; j++) {
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
                Sint32 wait_ms = app.needs_redraw ? 0 : -1;

                if (SDL_WaitEventTimeout(&event, wait_ms)) {
                        handle_event(&event, &app);
                }

                if (app.needs_redraw) {
                        render(renderer, &app);
                        app.needs_redraw = false;
                        // last_render = now;
                }
        }

        return 0;
}

