/* The game logic, 0x100e6328-0x100e663c: spreading a remaining distance over a walk. */
#include "iso_core.h"
#include "unitstepmove.h"

static int32_t iabs(int32_t v)
{
    return v < 0 ? -v : v;
}

/* one axis of path_step */
static void plan_axis(int32_t frames, int16_t rem, int16_t *count, int16_t *step, int16_t *period)
{
    if (!rem) {
        *step = 0;
        return;
    }
    int32_t r = rem, sign = r / iabs(r);
    *period = (int16_t)(frames / iabs(r));
    if (!*period && frames) {
        *period = 1;
        *step = (int16_t)(r / frames);
        if (r % frames)
            *step = (int16_t)(*step + sign);
    } else {
        *step = (int16_t)sign;
    }
    *count = *period;
}

/* 0x100e6328: spreads the remaining distance rem[0], rem[1] over `frames` frames: each axis
 * moves step[i] every period[i] frames, counted down in count[i]. A negative count[0] on
 * entry becomes 0. `id` is not used. Returns 1 */
int32_t __cdecl path_step(uint32_t id, int32_t frames, int16_t *first, const int16_t *rem, int16_t *count,
                          int16_t *step, int16_t *period)
{
    (void)id;
    if (first[0] < 0)
        first[0] = 0;
    plan_axis(frames, rem[0], &count[0], &step[0], &period[0]);
    plan_axis(frames, rem[1], &count[1], &step[1], &period[1]);
    return 1;
}

/* one axis of path_move */
static void move_axis(int16_t *rem, int16_t *count, int16_t *step, int16_t period, int16_t *pos, int32_t *total)
{
    if (!*rem)
        return;
    if (*count != 1) {
        (*count)--;
        return;
    }
    *pos = (int16_t)(*step + *pos);
    *total += *step;
    *rem = (int16_t)(*rem - *step);
    if (iabs(*step) > iabs(*rem))
        *step = *rem;
    *count = period;
}

/* 0x100e64f3: one frame of that walk: an axis whose count has run down moves by its step
 * (in *x or *y, added to *tx or *ty), and its last step is cut to what remains. Returns 1 */
int32_t __cdecl path_move(int16_t *rem, int16_t *count, int16_t *step, const int16_t *period, int16_t *x, int16_t *y,
                          int32_t *tx, int32_t *ty)
{
    move_axis(&rem[0], &count[0], &step[0], period[0], x, tx);
    move_axis(&rem[1], &count[1], &step[1], period[1], y, ty);
    return 1;
}
