#include <bal/hw.h>
#include <brutal/codec/tga/tga.h>
#include <brutal/gfx.h>
#include "init/boot.h"

void boot_splashscreen(Handover const *handover)
{
    if (!handover->framebuffer.present)
    {
        return;
    }

    // Open the framebuffer

    HandoverFramebuffer const *fb = &handover->framebuffer;

    size_t fb_size = align_up$(fb->height * fb->pitch, MEM_PAGE_SIZE);

    BalFb fb_surface;
    bal_fb_init_pmm(&fb_surface, fb->addr, fb_size, fb->width, fb->height, fb->pitch, GFX_FMT_BGRA8888);
    bal_fb_map(&fb_surface);

    // Open the bootimage

    HandoverModule const *img = handover_find_module(handover, str$("bootimg"));
    assert_not_null(img);

    BalMem img_mem;
    bal_mem_init_pmm(&img_mem, img->addr, img->size);
    bal_mem_map(&img_mem);

    GfxBuf img_surface = tga_decode_in_memory((void *)img_mem.buf, img_mem.len);

    // Display the image

    gfx_ops_clear(bal_fb_buf(&fb_surface), GFX_BLACK);

    gfx_ops_copy(
        bal_fb_buf(&fb_surface),
        img_surface,
        fb_surface.width / 2 - img_surface.width / 2,
        fb_surface.height / 2 - img_surface.height / 2);

    // Cleanup

    bal_mem_deinit(&img_mem);
    bal_fb_deinit(&fb_surface);
}
