
 // Yaroslava Sirychenko 251ADB151
// https://github.com/<ysiryyych>/calc-lab-home
// Compile with: gcc -O2 -Wall -Wextra -std=c17 -o calc calc.c -lm
// Resources used: <every AI tool, website and book you used; details in comments where you used them>

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>

#define NAME "yaroslava"
#define LASTNAME "sirychenko"
#define STUDENTID "251adb151"

static char *read_file(const char *path, size_t *len_out) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;

    size_t cap = 4096, len = 0;
    char *buf = malloc(cap);
    if (!buf) {
        fclose(f);
        return NULL;
    }

    size_t n;
    while ((n = fread(buf + len, 1, cap - len - 1, f)) > 0) {
        len += n;

        if (len + 1 >= cap) {
            if (cap > (size_t)-1 / 2) {
                free(buf);
                fclose(f);
                return NULL;
            }

            cap *= 2;
            char *tmp = realloc(buf, cap);
            if (!tmp) {
                free(buf);
                fclose(f);
                return NULL;
            }
            buf = tmp;
        }
    }

    if (ferror(f)) {
        free(buf);
        fclose(f);
        return NULL;
    }

    fclose(f);
    buf[len] = '\0';
    *len_out = len;
    return buf;
}

typedef struct {
    const char *s;
    size_t pos;
    size_t end;
    size_t error_pos;
} Parser;

static void skip_spaces(Parser *p) {
    while (p->pos < p->end &&
           (p->s[p->pos] == ' ' ||
            p->s[p->pos] == '\t' ||
            p->s[p->pos] == '\r')) {
        p->pos++;
    }
}

static long long parse_expression(Parser *p);
static long long parse_power(Parser *p);


static long long parse_primary(Parser *p) {
    skip_spaces(p);

    if (p->pos >= p->end) {
        p->error_pos = p->end + 1;
        return 0;
    }

    if (p->s[p->pos] == '(') {
        p->pos++;

        long long value = parse_expression(p);
        if (p->error_pos != 0) return 0;

        skip_spaces(p);

        if (p->pos >= p->end) {
            p->error_pos = p->end + 1;
            return 0;
        }

        if (p->s[p->pos] != ')') {
            p->error_pos = p->pos + 1;
            return 0;
        }

        p->pos++;
        return value;
    }

    if (!isdigit((unsigned char)p->s[p->pos])) {
        p->error_pos = p->pos + 1;
        return 0;
    }

    long long value = 0;

    while (p->pos < p->end &&
           isdigit((unsigned char)p->s[p->pos])) {
        value = value * 10 + (p->s[p->pos] - '0');
        p->pos++;
    }

    return value;
}

static long long integer_power(long long base, long long exponent) {
    long long result = 1;

    while (exponent > 0) {
        if (exponent % 2 != 0) {
            result *= base;
        }

        exponent /= 2;

        if (exponent > 0) {
            base *= base;
        }
    }

    return result;
}

static long long parse_power(Parser *p) {
    long long value = parse_primary(p);
    if (p->error_pos != 0) return 0;

    skip_spaces(p);

    if (p->pos + 1 >= p->end ||
        p->s[p->pos] != '*' ||
        p->s[p->pos + 1] != '*') {
        return value;
    }

    p->pos += 2;
    skip_spaces(p);

    size_t exponent_start = p->pos;

    if (p->pos < p->end && p->s[p->pos] == '-') {
        p->error_pos = p->pos + 1;
        return 0;
    }

    long long exponent = parse_power(p);
    if (p->error_pos != 0) return 0;

    if (exponent < 0) {
        p->error_pos = exponent_start + 1;
        return 0;
    }

    return integer_power(value, exponent);
}

static long long parse_term(Parser *p) {
    long long value = parse_power(p);
    if (p->error_pos != 0) return 0;

    while (1) {
        skip_spaces(p);

        if (p->pos >= p->end ||
            (p->s[p->pos] != '*' && p->s[p->pos] != '/')) {
            break;
        }

        char op = p->s[p->pos++];

        if (op == '*' && p->pos < p->end && p->s[p->pos] == '*') {
            p->error_pos = p->pos + 1;
            return 0;
        }

        skip_spaces(p);
        size_t right_start = p->pos;

        long long right = parse_power(p);
        if (p->error_pos != 0) return 0;

        if (op == '*') {
            value *= right;
        } else {
            if (right == 0) {
                p->error_pos = right_start + 1;
                return 0;
            }

            value /= right;
        }
    }

    return value;
}

static long long parse_expression(Parser *p) {
    long long value = parse_term(p);
    if (p->error_pos != 0) return 0;

    while (1) {
        skip_spaces(p);

        if (p->pos >= p->end ||
            (p->s[p->pos] != '+' && p->s[p->pos] != '-')) {
            break;
        }

        char op = p->s[p->pos++];
        long long right = parse_term(p);
        if (p->error_pos != 0) return 0;

        if (op == '+') {
            value += right;
        } else {
            value -= right;
        }
    }

    return value;
}

static long long eval_line(const char *s, size_t start, size_t end,
                           size_t *error_pos) {
    Parser p = {s, start, end, 0};

    long long result = parse_expression(&p);

    if (p.error_pos == 0) {
        skip_spaces(&p);

        if (p.pos < p.end) {
            p.error_pos = p.pos + 1;
        }
    }

    *error_pos = p.error_pos;
    return (*error_pos != 0) ? 0 : result;
}

static void process_text(char *buf, size_t len, FILE *out) {
    FILE *temp = tmpfile();

    if (!temp) {
        fprintf(stderr, "cannot create temporary file\n");
        return;
    }

    size_t i = 0;
    size_t error_pos = 0;

    while (i < len) {
        char *nl = memchr(buf + i, '\n', len - i);
        size_t end = nl ? (size_t)(nl - buf) : len;

        size_t j = i;
        while (j < end &&
               (buf[j] == ' ' || buf[j] == '\t' || buf[j] == '\r')) {
            j++;
        }

        /* Ignore blank lines and comment lines. */
        if (j < end && buf[j] != '#') {
            size_t line_error = 0;
            long long result = eval_line(buf, j, end, &line_error);

            if (line_error != 0) {
                error_pos = line_error;
                break;
            }

            fprintf(temp, "%lld\n", result);
        }

        i = nl ? end + 1 : len;
    }

    if (error_pos != 0) {
        fprintf(out, "ERROR:%zu\n", error_pos);
    } else {
        rewind(temp);

        int ch;
        while ((ch = fgetc(temp)) != EOF) {
            fputc(ch, out);
        }
    }

    fclose(temp);
}

static const char *base_name(const char *path) {
    const char *slash = strrchr(path, '/');
    return slash ? slash + 1 : path;
}

static int ends_with_txt(const char *name) {
    size_t len = strlen(name);
    return len >= 4 && strcmp(name + len - 4, ".txt") == 0;
}

static char *make_path(const char *dir, const char *name) {
    size_t size = strlen(dir) + strlen(name) + 2;
    char *path = malloc(size);

    if (path) {
        snprintf(path, size, "%s/%s", dir, name);
    }

    return path;
}

static int ensure_dir(const char *dir) {
    struct stat st;

    if (stat(dir, &st) == 0) {
        if (S_ISDIR(st.st_mode)) {
            return 0;
        }

        fprintf(stderr, "%s is not a directory\n", dir);
        return -1;
    }

    if (mkdir(dir, 0777) == 0) {
        return 0;
    }

    fprintf(stderr, "cannot create folder %s\n", dir);
    return -1;
}

static int process_file(const char *input, const char *outdir) {
    size_t len = 0;
    char *text = read_file(input, &len);

    if (!text) {
        fprintf(stderr, "cannot read %s\n", input);
        return -1;
    }

    const char *base = base_name(input);
    size_t base_len = strlen(base);

    if (ends_with_txt(base)) {
        base_len -= 4;
    }

    int name_len = snprintf(NULL, 0,
        "%.*s_%s_%s_%s.txt",
        (int)base_len, base, NAME, LASTNAME, STUDENTID);

    if (name_len < 0) {
        free(text);
        return -1;
    }

    char *filename = malloc((size_t)name_len + 1);

    if (!filename) {
        free(text);
        return -1;
    }

    snprintf(filename, (size_t)name_len + 1,
        "%.*s_%s_%s_%s.txt",
        (int)base_len, base, NAME, LASTNAME, STUDENTID);

    char *path = make_path(outdir, filename);
    free(filename);

    if (!path) {
        free(text);
        return -1;
    }

    FILE *out = fopen(path, "wb");

    if (!out) {
        fprintf(stderr, "cannot write %s\n", path);
        free(path);
        free(text);
        return -1;
    }

    process_text(text, len, out);

    int failed = ferror(out);
    if (fclose(out) != 0) {
        failed = 1;
    }

    free(path);
    free(text);
    return failed ? -1 : 0;
}

static int process_dir(const char *indir, const char *outdir) {
    DIR *dir = opendir(indir);

    if (!dir) {
        fprintf(stderr, "cannot open folder %s\n", indir);
        return -1;
    }

    char **files = NULL;
    size_t count = 0;
    size_t capacity = 0;
    struct dirent *entry;
    int failed = 0;

    while ((entry = readdir(dir)) != NULL) {
        if (!ends_with_txt(entry->d_name)) {
            continue;
        }

        char *path = make_path(indir, entry->d_name);

        if (!path) {
            failed = 1;
            break;
        }

        struct stat st;

        if (stat(path, &st) != 0 || !S_ISREG(st.st_mode)) {
            free(path);
            continue;
        }

        free(path);

        if (count == capacity) {
            size_t new_capacity = capacity ? capacity * 2 : 8;
            char **tmp = realloc(files,
                                 new_capacity * sizeof(*files));

            if (!tmp) {
                failed = 1;
                break;
            }

            files = tmp;
            capacity = new_capacity;
        }

        size_t name_len = strlen(entry->d_name) + 1;
files[count] = malloc(name_len);

if (files[count] != NULL) {
    memcpy(files[count], entry->d_name, name_len);
}

        if (!files[count]) {
            failed = 1;
            break;
        }

        count++;
    }

    closedir(dir);

    /* Finish collecting inputs before creating output files. */
    for (size_t i = 0; i < count; i++) {
        if (!failed) {
            char *input = make_path(indir, files[i]);

            if (!input || process_file(input, outdir) != 0) {
                failed = 1;
            }

            free(input);
        }

        free(files[i]);
    }

    free(files);
    return failed ? -1 : 0;
}

int main(int argc, char **argv) {
    const char *indir = NULL;
    const char *input = NULL;
    const char *outdir = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-d") == 0 ||
            strcmp(argv[i], "--dir") == 0) {
            if (indir || i + 1 >= argc ||
                argv[i + 1][0] == '-') {
                goto usage_error;
            }

            indir = argv[++i];
        } else if (strcmp(argv[i], "-o") == 0 ||
                   strcmp(argv[i], "--output-dir") == 0) {
            if (outdir || i + 1 >= argc ||
                argv[i + 1][0] == '-') {
                goto usage_error;
            }

            outdir = argv[++i];
        } else if (argv[i][0] == '-') {
            goto usage_error;
        } else {
            if (input) {
                goto usage_error;
            }

            input = argv[i];
        }
    }

    if ((indir && input) || (!indir && !input)) {
        goto usage_error;
    }

    char *default_outdir = NULL;

    if (!outdir) {
        const char *base = base_name(indir ? indir : input);
        size_t len = strlen(base);

        if (!indir && ends_with_txt(base)) {
            len -= 4;
        }

        int n = snprintf(NULL, 0, "%.*s_%s_%s",
                         (int)len, base, NAME, STUDENTID);

        if (n < 0) {
            return 1;
        }

        default_outdir = malloc((size_t)n + 1);

        if (!default_outdir) {
            return 1;
        }

        snprintf(default_outdir, (size_t)n + 1,
                 "%.*s_%s_%s",
                 (int)len, base, NAME, STUDENTID);

        outdir = default_outdir;
    }

    if (ensure_dir(outdir) != 0) {
        free(default_outdir);
        return 1;
    }

    int result = indir
        ? process_dir(indir, outdir)
        : process_file(input, outdir);

    free(default_outdir);
    return result == 0 ? 0 : 1;

usage_error:
    fprintf(stderr,
        "usage: calc [-d DIR | --dir DIR] "
        "[-o OUTDIR | --output-dir OUTDIR] [input.txt]\n");
    return 1;
}