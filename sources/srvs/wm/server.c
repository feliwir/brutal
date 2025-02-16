#include <brutal/alloc.h>
#include <brutal/gfx.h>
#include <protos/bus.h>
#include <protos/wm.h>
#include "wm/server.h"

/* --- Input Sink Protocol -------------------------------------------------- */

static EventError input_sink_dispatch_handler(void *ctx, UiEvent const *req, bool *resp, Alloc *)
{
    WmServer *wm = ctx;
    wm_server_dispatch(wm, *req);

    *resp = true;
    return IPC_SUCCESS;
}

static EventSinkVTable _input_sink_vtable = {
    input_sink_dispatch_handler,
};

/* --- Window Manager Server Protocol --------------------------------------- */

WmError wm_server_create_handler(void *self, WmClientProps const *req, IpcCap *resp, Alloc *)
{
    WmServer *server = self;

    WmClient *client = wm_client_create(server, req->bound, req->type);
    vec_push(&server->clients, client);
    *resp = client->wm_client;

    return IPC_SUCCESS;
}

WmServerVTable _wm_server_vtable = {
    wm_server_create_handler,
};

/* --- Render loop ---------------------------------------------------------- */

static void *wm_render_fiber(void *ctx)
{
    WmServer *self = ctx;

    while (true)
    {

        if (self->layout_dirty)
        {
            wm_server_layout(self);
        }

        if (gfx_dirty_any(&self->display_dirty))
        {
            wm_server_render(self);
        }

        fiber_sleep(16);
    }

    return nullptr;
}

/* --- Window Manager Server ------------------------------------------------ */

void wm_server_init(WmServer *self, WmDisplay *display)
{
    self->display = display;
    self->mouse = m_vec2f(0, 0);
    gfx_dirty_init(&self->display_dirty, alloc_global());
    vec_init(&self->clients, alloc_global());

    self->wm_server = wm_server_provide(ipc_component_self(), &_wm_server_vtable, self);
    self->input_sink = event_sink_provide(ipc_component_self(), &_input_sink_vtable, self);
    self->render_fiber = fiber_start(wm_render_fiber, self);
}

void wm_server_deinit(WmServer *self)
{
    ipc_component_revoke(ipc_component_self(), self->wm_server);
    ipc_component_revoke(ipc_component_self(), self->input_sink);

    vec_deinit(&self->clients);
    gfx_dirty_deinit(&self->display_dirty);
}

void wm_server_dispatch(WmServer *self, UiEvent event)
{
    if (ui_event_is_mouse(&event))
    {
        if (event.type == UI_EVENT_MOUSE_MOVE)
        {
            MRectf mouse_bound = {
                .pos = m_vec2f_sub(self->mouse, m_vec2f(8, 8)),
                .size = m_vec2f(42, 42),
            };

            wm_server_should_render(self, mouse_bound);

            self->mouse = m_vec2f_add(self->mouse, event.mouse.offset);
            self->mouse = m_rectf_clamp_vec2(wm_display_bound(self->display), self->mouse);

            mouse_bound = (MRectf){
                .pos = self->mouse,
                .size = m_vec2f(32, 32),
            };

            wm_server_should_render(self, mouse_bound);
        }

        event.mouse.position = self->mouse;
    }

    WmClient *client = wm_server_client_at(self, self->mouse);
    if (client)
    {
        wm_client_dispatch(client, event);
    }
}

static void wm_server_render_cursor(WmServer *self, Gfx *gfx)
{
    gfx_push(gfx);
    gfx_origin(gfx, m_vec2f_sub(self->mouse, m_vec2f(1, 1)));
    gfx_fill_style(gfx, gfx_paint_fill(GFX_BLACK));
    gfx_fill_svg(gfx, str$("M 0 0 L 16 12.279 L 9.049 13.449 L 13.374 22.266 L 9.778 24 L 5.428 15.121 L -0 19.823 Z"), GFX_FILL_EVENODD);

    gfx_stroke_style(gfx, (GfxStroke){.pos = GFX_STOKE_OUTSIDE, .width = 1});
    gfx_fill_style(gfx, gfx_paint_fill(UI_COLOR_BASE09));
    gfx_stroke(gfx);

    gfx_pop(gfx);
}

static void wm_server_render_clients(WmServer *self, Gfx *gfx)
{
    vec_foreach_v(client, &self->clients)
    {
        GfxBuf backbuffer = wm_client_backbuffer(client);
        gfx_fill_style(gfx, gfx_paint_image(backbuffer, gfx_buf_bound(backbuffer)));

        if (client->type == UI_WIN_NORMAL)
        {
            gfx_fill_rect(gfx, client->bound, 8);
        }
        else
        {
            gfx_fill_rect(gfx, client->bound, 0);
        }

        gfx_stroke_style(gfx, (GfxStroke){.pos = GFX_STOKE_INSIDE, .width = 1});
        gfx_fill_style(gfx, gfx_paint_fill(UI_COLOR_BASE02));
        gfx_stroke(gfx);
    }
}

void wm_server_should_layout(WmServer *self)
{
    self->layout_dirty = true;
}

void wm_server_layout(WmServer *self)
{
    self->layout_dirty = false;
}

void wm_server_should_render(WmServer *self, MRectf rect)
{
    gfx_dirty_rect(&self->display_dirty, rect);
}

void wm_server_should_render_all(WmServer *self)
{
    wm_server_should_render(self, wm_display_bound(self->display));
}

void wm_server_render(WmServer *self)
{
    Gfx *gfx = wm_display_begin(self->display);

    gfx_dirty_foreach(dirty, &self->display_dirty)
    {
        gfx_push(gfx);
        gfx_clip(gfx, dirty);

        gfx_clear(gfx, GFX_GAINSBORO);

        wm_server_render_clients(self, gfx);
        wm_server_render_cursor(self, gfx);

        // gfx_fill_style(gfx, gfx_paint_fill(gfx_color_rand(50)));
        // gfx_fill_rect(gfx, dirty, 0);

        gfx_pop(gfx);
    }

    gfx_dirty_foreach(dirty, &self->display_dirty)
    {
        wm_display_flip(self->display, dirty);
    }

    wm_display_end(self->display);

    gfx_dirty_clear(&self->display_dirty);
}

void wm_server_expose(WmServer *self, IpcCap bus_server)
{
    bus_server_expose_rpc(ipc_component_self(), bus_server, &self->input_sink, alloc_global());
    bus_server_expose_rpc(ipc_component_self(), bus_server, &self->wm_server, alloc_global());
}

WmClient *wm_server_client_at(WmServer *self, MVec2f vec)
{
    vec_foreach_v(client, &self->clients)
    {
        if (m_rectf_collide_point(client->bound, vec))
        {
            return client;
        }
    }

    return nullptr;
}
