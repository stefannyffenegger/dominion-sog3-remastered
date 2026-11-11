/* Engine core, 0x10025130-0x10025880: sprite transfer (RtSpriteTransfer): one sprite
 * object takes over another's place and images, with reference counts on the images
 * they use. Rewritten from the original. */
#include "rtcore.h"
#include "rt.h"
#include "rt_transfer.h"

#define RU(off) R(U, off)
#define HSL R(uint8_t *, RT_HSL)
#define RES R(ResFile *, RT_RES)
#define RT_TOPMOST 0x0cd4
#define RT_FIRST_COPY 0x3dfb
#define C1(a, x) ORIG(U(__stdcall *)(U), a)((U)(x))

/* SpriteGetInfo's block (0x5c bytes): +4 the images, +0x2c their number */
#define INFO_SIZE 0x5c
#define info_count(i) P(U, i, 0x2c)
#define info_image(i, n) P(uint8_t *, i, 4 + (n) * 4)
#define image_id(b) P(U, b, 0x1c)
/* a sprite resource: +8 its own images, +0xc all images, +0x26 their ids */
#define spr_own(s) P(U, s, 8)
#define spr_count(s) P(U, s, 0xc)
#define spr_ids(s) ((U *)((s) + 0x26))

static uint8_t *sprite_of(U id) { return (uint8_t *)(uintptr_t)obj_entry(id)[1]; }

/* 0x100251d4: drops one reference to resource `id`; the last one releases it */
void __stdcall res_unref(U id)
{
    U *e = obj_entry(id);
    if (e[0] && e[1] && --e[1] == 0)
        obj_release(id, ResGetType(RES, id));
}

/* 0x10025130: drops the images added to sprite resource `id` beyond its own */
void __stdcall sprite_extra_release(U id)
{
    ResGetSize(RES, id);
    uint8_t *s = (uint8_t *)(uintptr_t)obj_entry(id)[0];
    U n = spr_count(s), own = spr_own(s);
    if (own >= n)
        return;
    for (U i = own; i < n; i++)
        res_unref(spr_ids(s)[i]);
    spr_count(s) = own;
}

/* 0x100254ef: before the old sprite goes: its shown images gain a reference and the
 * resource's own ones are released */
void __stdcall sprite_images_keep(U id, const uint8_t *info)
{
    SpriteEnable(sprite_of(id), 0);
    for (U i = 0; i < info_count(info); i++)
        obj_entry(image_id(info_image(info, i)))[1]++;
    uint8_t *s = (uint8_t *)(uintptr_t)obj_entry(id)[0];
    U n = spr_count(s);
    for (U i = 0; i < n; i++) {
        U img = spr_ids(s)[i];
        U type = ResGetType(RES, img);
        if (type == 1 || type == 0xd || type == 0x15 || type == 0xf)
            res_unref(img);
        else
            obj_release(img, type);
    }
}

/* 0x1002562d: destroys the old sprite and drops its images' references */
void __stdcall sprite_free_old(U id, const uint8_t *info)
{
    U *e = obj_entry(id);
    uint8_t *s = (uint8_t *)(uintptr_t)e[1];
    if (R(uint8_t *, RT_TOPMOST) == s)
        RU(RT_TOPMOST) = 0;
    SpriteDestroy(HSL, s, 1, 0);
    ((uint8_t *)e)[8] = 0;
    e[1] = 0;
    for (U i = 0; i < info_count(info); i++)
        res_unref(image_id(info_image(info, i)));
    if (id >= RU(RT_FIRST_COPY))
        obj_delete(id);
}

/* 0x10025703: the new sprite resource takes the old sprite's images */
U __stdcall sprite_images_take(U id, const uint8_t *info)
{
    sprite_extra_release(id);
    U count = info_count(info);
    if (!count)
        return 1;
    U *e = obj_entry(id);
    uint8_t *res = (uint8_t *)(uintptr_t)e[0];
    U total = spr_count(res) + count;
    U *ids = spr_ids(res) + total - count;
    uint8_t *s = (uint8_t *)(uintptr_t)e[1];
    for (U i = 0; i < count; i++) {
        ids[i] = image_id(info_image(info, i));
        obj_entry(ids[i])[1]++;
        if (ResGetType(RES, ids[i]) == 0xd)
            C1(0x100167d0, obj_entry(ids[i])[2]);
        if (R(uint8_t *, RT_TOPMOST) == s && !SpriteAppendBitmap(HSL, s, (U)(uintptr_t)info_image(info, i)))
            return 0;
    }
    spr_count(res) = total;
    if (R(uint8_t *, RT_TOPMOST) != s)
        SpriteCallBack(s, 1);
    return 1;
}

/* 0x1002535e: moves sprite `from` into `to`'s place */
U __stdcall sprite_swap(U from, U to, U destroy)
{
    uint8_t info_from[INFO_SIZE], info_to[INFO_SIZE];
    uint8_t *s = sprite_of(from);
    SpriteGetInfo(s, (uint32_t *)info_from);
    SpriteEmptyQueue(HSL, s);
    SpriteClearUpdate(s);
    if (destroy)
        sprite_images_keep(from, info_from);
    U *e = obj_entry(to);
    int had = e[1] != 0;
    if (!had && !obj_is(to, 0x14))
        return 0;
    uint8_t *s2 = (uint8_t *)(uintptr_t)e[1];
    SpriteGetInfo(s2, (uint32_t *)info_to);
    if ((had && P(U, info_to, 0x28)) || !sprite_images_take(to, info_from)) {
        if (!destroy)
            SpriteHide(HSL, s, 1);
    } else {
        SpriteTransfer(s, s2);
    }
    SpriteSwapOrder(HSL, (U)(uintptr_t)s, (U)(uintptr_t)s2, 1, 0);
    if (destroy)
        sprite_free_old(from, info_from);
    return 1;
}

/* 0x1002524b: RtSpriteTransfer: sprite `to` takes over from `from` (which is freed
 * with `destroy`) and gets `queue` */
U __stdcall sprite_transfer(U from, U to, U queue, U destroy)
{
    U ok = obj_check_type(from, 0x14) && obj_check_type(to, 0x14);
    if (!ok)
        return 0;
    if (!obj_entry(from)[1])
        return 1;
    if (to != from) {
        ok = sprite_swap(from, to, destroy);
        if (destroy && obj_entry(from)[1]) {
            if (from < RU(RT_FIRST_COPY))
                obj_release(from, 0x14);
            else
                obj_delete(from);
        }
    }
    return ok && cmd_set_queue(to, queue, 0);
}
