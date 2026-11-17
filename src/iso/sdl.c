/* The game logic, 0x10025747-0x10026aac: loading a scenario description (IsoLoadSDLFile:
 * the .sdc text file next to the map): one 0x5a-byte record per command line, read in
 * two passes (count, then parse). Multiplayer clients use another loader (0x1002cf1c).
 * All C runtime calls are the original's, so that the FILE objects, strtok's state and
 * the parsing stay the same. */
#include "iso_core.h"
#include "aiglue.h"
#include "sdl.h"
#include "sdlexec.h"
#include "sdcget.h"

#define g_fopen ORIG(void *(__cdecl *)(const char *, const char *), 0x100f5e30)
#define g_fclose ORIG(int(__cdecl *)(void *), 0x100f5e60)
#define g_fseek ORIG(int(__cdecl *)(void *, long, int), 0x100f6d80)
#define g_fgets ORIG(char *(__cdecl *)(char *, int, void *), 0x100f6c30)
#define g_fputs ORIG(int(__cdecl *)(const char *, void *), 0x100f69b0)
#define g_strchr ORIG(char *(__cdecl *)(const char *, int), 0x100f6b70)
#define g_strpbrk ORIG(char *(__cdecl *)(const char *, const char *), 0x100f6aa0)
#define g_strncmp ORIG(int(__cdecl *)(const char *, const char *, size_t), 0x100f6b20)
#define g_strlen ORIG(size_t(__cdecl *)(const char *), 0x100f5750)
#define g_strcpy ORIG(char *(__cdecl *)(char *, const char *), 0x100f4c70)
#define g_strtok ORIG(char *(__cdecl *)(char *, const char *), 0x100f5fc0)
#define g_strcmpi ORIG(int(__cdecl *)(const char *, const char *), 0x100f5680)
#define g_atoi ORIG(int(__cdecl *)(const char *), 0x100f6260)
#define g_isalpha ORIG(int(__cdecl *)(int), 0x100f64f0)
#define g_bsearch ORIG(void *(__cdecl *)(const void *, const void *, size_t, size_t, void *), 0x100f63d0)
#define g_memset ORIG(void *(__cdecl *)(void *, int, size_t), 0x100f4c10)
#define message_box ORIG(void(__cdecl *)(int, const char *, const char *, int), 0x1005c515)
#define sdl_record_check sdl_exec
#define sdl_load_client sdc_load
#define sdl_start ORIG(void(__stdcall *)(uint8_t *), 0x10048270)

#define ISO_RECORDS 0xc2e5   /* the records: +0 type, +4 subtype, +8 short count, +10 values */
#define ISO_RECORD_COUNT 0xc2e9
#define TEXT_BUF ((char *)(g_iso + 0xdea))
#define S(addr) ((const char *)ORIG_PTR(addr))
#define REC(i) (I(uint8_t *, ISO_RECORDS) + (i) * 0x5a)

/* the commands: 64 of 0x1e bytes: name, type (0: the previous record's), subtype, the
 * number of values (negative: strings), the number required */
#define CMD(k) ((const uint8_t *)ORIG_PTR(0x101320e0 + (k) * 0x1e))

static int is_eof(void *f) { return P(int32_t, f, 0xc) & 0x10; }

/* 0x100257f2: `file` in folder `dir`; `check` checks the records as they are read */
int __cdecl sdl_load(char *file, char *dir, int check)
{
    char path[260], raw[0x400], token[0x400];
    void *ai_file = NULL;
    uint16_t lines = 0, rec = 0;
    uint8_t briefs = 0;
    DebugStr(S(0x10134808));
    if (I(int, 0x175) && !I(int32_t, 0xd9da)) {
        file[g_strlen(file) - 1] = 'C';
        return sdl_load_client(file, dir, check);
    }
    int32_t last_type = 0, last_sub = 0, text = 0, plain = 1, text_lines = 0;
    G(int32_t, 0x1017bb58) = 0;
    G(int32_t, 0x1017bb60) = G(int32_t, 0x1017bb58);
    G(int32_t, 0x1017bb5c) = 1;
    G(int32_t, 0x1017bb30) = G(int32_t, 0x1017bb5c);
    for (rec = 0; rec < 8; rec++) {
        G(int32_t, 0x1017bb38 + rec * 4) = 1;
        G(int32_t, 0x1017bb10 + rec * 4) = G(int32_t, 0x1017bb38 + rec * 4);
    }
    if (!I(int32_t, ISO_HAVE_SPRITES)) {
        IsoInitMapEditAITempFiles();
        I(int32_t, 0xdaae) = 1;
    }
    I(int32_t, 0x1918) = 0;
    I(int32_t, 0x1914) = 0;
    I(int32_t, 0x18d8) = 0;
    I(int32_t, 0x1928) = 0;
    g_strcpy((char *)(g_iso + 0xc0dd), S(0x1013481c));
    for (rec = 0; rec < 0x1e; rec++)
        I(int32_t, 0x1860 + rec * 4) = 0;
    for (rec = 0; rec < 3; rec++)
        I(int32_t, 0x1908 + rec * 4) = 0;
    ai_names_sort();
    g_strcpy((char *)(g_iso + 0xbfd9), file);
    g_strcpy((char *)(g_iso + 0xc1e1), dir);
    wsprintfA(path, S(0x101348a0), g_iso + 0xc1e1, g_iso + 0xbfd9);
    void *f = g_fopen(path, S(0x101348a8));
    if (!f) {
        char title[128];
        wsprintfA(TEXT_BUF, S(0x101348ac), path);
        DebugStr(TEXT_BUF);
        wsprintfA(title, S(0x101348c0), path);
        message_box(0, TEXT_BUF, title, 0);
        return 0;
    }
    for (int pass = 1; pass <= 2; pass++) {
        lines = 0;
        rec = 0;
        g_fseek(f, 0, SEEK_SET);
        while (!is_eof(f)) {
            int quoted = 0;
            char *s = raw, *t = token;
            text = 0;
            plain = 1;
            g_memset(s, 0, 0x400);
            g_memset(t, 0, 0x400);
            g_fgets(s, 0x400, f);
            char *nl = g_strchr(s, '\n');
            if (nl)
                *nl = 0;
            if (!is_eof(f))
                lines++;
            if (text_lines) { /* a line of a text block (briefing) */
                text = 1;
                plain = 0;
            }
            if (text && *s) {
                if (*s == '#')
                    g_strcpy(s, S(0x101348d8));
                g_strcpy((char *)REC(rec) + 10, s);
                P(int32_t, REC(rec), 0) = P(int32_t, REC(rec - 1), 0);
                P(int32_t, REC(rec), 4) = 5;
                P(int16_t, REC(rec), 8) = 0;
                text_lines--;
                if (pass == 2 && (P(int32_t, REC(rec), 0) == 0x32 || P(int32_t, REC(rec), 0) == 0x3c)) {
                    briefs++;
                    I(uint8_t *, 0x1860 + briefs * 4) = REC(rec) + 10;
                    DebugStr(S(0x101348dc), briefs, I(uint8_t *, 0x1860 + briefs * 4));
                }
            }
            if (!I(int32_t, ISO_HAVE_SPRITES)) { /* the editor copies AI rule sets to temp files */
                if (pass == 1) {
                    if (!g_strncmp(s, S(0x101348f8), 10)) {
                        char *n = g_strpbrk(s, S(0x10134904));
                        int k = n ? g_atoi(n + 1) : 0;
                        if (ai_file)
                            g_fclose(ai_file);
                        ai_file = ai_temp_file(k, 5);
                    } else if (ai_file) {
                        g_fputs(s, ai_file);
                        g_fputs(S(0x10134908), ai_file);
                    }
                } else if (ai_file) {
                    g_fclose(ai_file); /* the original closes it again on every line */
                }
            }
            /* the line without blanks (outside quotes, unless in text), quotes and comments */
            while (*s) {
                int quote = *s == '"';
                if (quote)
                    quoted = !quoted;
                if ((*s == ' ' && plain && !quoted) || *s == '\r' || *s == '\n' || quote) {
                    s++;
                    continue;
                }
                if (*s == '\r' || *s == '\n' || *s == '/' || *s == ';')
                    break;
                *t++ = *s++;
            }
            *t = 0;
            t = token;
            if (!*t)
                continue; /* an empty line takes no record */
            if (pass == 2) {
                if (!text)
                    DebugStr(S(0x1013490c), t);
                char *cmd = g_strtok(t, S(0x10134914));
                if (!cmd) {
                    char title[128];
                    wsprintfA(TEXT_BUF, S(0x10134918), t);
                    DebugStr(TEXT_BUF);
                    wsprintfA(title, S(0x10134930), file);
                    message_box(0, TEXT_BUF, title, 0);
                    if (ai_file)
                        g_fclose(ai_file);
                    return 0;
                }
                int known = 0;
                for (int k = 0; k < 0x40; k++) {
                    if (g_strcmpi((const char *)CMD(k), cmd))
                        continue;
                    known = 1;
                    P(int32_t, REC(rec), 0) = P(int32_t, CMD(k), 0x14) ? P(int32_t, CMD(k), 0x14) : last_type;
                    P(int32_t, REC(rec), 4) = P(int32_t, CMD(k), 0x18);
                    int8_t np = P(int8_t, CMD(k), 0x1c);
                    int n = np < 0 ? -np : np;
                    if (!n)
                        continue;
                    char *v = g_strtok(NULL, n > 1 ? S(0x10134948) : S(0x1013494c));
                    for (int i = 0; i < n; i++) {
                        if (!v) {
                            if (i < P(int8_t, CMD(k), 0x1d)) {
                                char title[128];
                                g_fclose(f);
                                wsprintfA(TEXT_BUF, S(0x10134950), lines, token);
                                DebugStr(TEXT_BUF);
                                wsprintfA(title, S(0x1013496c), file);
                                message_box(0, TEXT_BUF, title, 0);
                                return 0;
                            }
                            i = 99;
                            continue;
                        }
                        if (np < 0) { /* strings */
                            DebugStr(S(0x10134994), n > 1 ? S(0x10134984) : S(0x1013498c), v);
                            if (n < 2)
                                g_strcpy((char *)REC(rec) + 10, v);
                            else
                                g_strcpy((char *)REC(rec) + 10 + i * 0x14, v);
                        } else {
                            int named = 0, value = 0;
                            void *e;
                            if (g_isalpha(*v) && (e = g_bsearch(v, ORIG_PTR(0x10132860), 0xc9, 0x28, ORIG_PTR(0x100256ef)))) {
                                named = 1;
                                value = P(int32_t, e, 0x24);
                            }
                            if (!named)
                                value = g_atoi(v);
                            P(int32_t, REC(rec), 10 + i * 4) = value;
                        }
                        P(int16_t, REC(rec), 8)++;
                        v = g_strtok(NULL, n - i > 1 ? S(0x101349b0) : S(0x101349b4));
                    }
                    if (P(int32_t, REC(rec), 0) == 0x32 || P(int32_t, REC(rec), 0) == 0x3c) {
                        text = 1; /* a text block of n lines follows */
                        plain = 0;
                        text_lines = P(int32_t, REC(rec), 10);
                        briefs = 0;
                    }
                }
                if (!text_lines && text) {
                    text = 0;
                    plain = 1;
                    known = 1;
                }
                if (!text) {
                    if (!known) {
                        char title[128];
                        wsprintfA(TEXT_BUF, S(0x101349b8), cmd);
                        DebugStr(TEXT_BUF);
                        wsprintfA(title, S(0x101349d0), file);
                        message_box(0, TEXT_BUF, title, 0);
                        return 0;
                    }
                    last_type = P(int32_t, REC(rec), 0);
                    last_sub = P(int32_t, REC(rec), 4);
                    DebugStr(S(0x101349e8), last_type, last_sub);
                }
                if (check &&
                    ((last_type == 10 && !last_sub) || (last_type == 0x14 && !last_sub) || last_type == 0x28 ||
                     last_type == 0x32 || last_type == 0x3c || last_type == 0x46 || last_type == 0x50 ||
                     last_type == 0x5a) &&
                    !sdl_record_check(rec) && I(int32_t, ISO_HAVE_SPRITES)) {
                    winfree(I(void *, ISO_RECORDS));
                    I(void *, ISO_RECORDS) = NULL;
                    g_fclose(f);
                    return 0;
                }
            }
            rec++;
        }
        if (pass == 1) {
            if (I(void *, ISO_RECORDS)) {
                winfree(I(void *, ISO_RECORDS));
                I(void *, ISO_RECORDS) = NULL;
            }
            I(void *, ISO_RECORDS) = winmalloc((SIZE_T)(rec * 0x5a), 1);
            if (!I(void *, ISO_RECORDS)) {
                g_fclose(f);
                return 0;
            }
            I(uint16_t, ISO_RECORD_COUNT) = rec;
        }
    }
    DebugStr(S(0x10134a08), lines, rec);
    g_fclose(f);
    if (check && I(int32_t, ISO_HAVE_SPRITES) && !G(int32_t, 0x10139e68)) {
        if (I(int, 0x175) && !I(int32_t, 0x138))
            return 1;
        sdl_start(g_iso);
    }
    if (!I(int32_t, ISO_HAVE_SPRITES))
        I(int32_t, 0xdaae) = 0;
    return 1;
}

/* 0x10025747: loads the scenario description named by string `file_id` in the folder
 * of string `dir_id` */
int __cdecl IsoLoadSDLFile(uint8_t *iso, uint32_t file_id, uint32_t dir_id, int check)
{
    g_iso = iso;
    char *file = (char *)RtGetString(I(void *, ISO_APP), file_id);
    if (!file)
        return 0;
    char *dir = (char *)RtGetString(I(void *, ISO_APP), dir_id);
    if (!dir)
        return 0;
    DebugStr(S(0x10134800), file);
    return sdl_load(file, dir, check);
}
