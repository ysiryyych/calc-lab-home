
 // Yaroslava Sirychenko 251ADB151
// https://github.com/<ysiryyych>/calc-lab-home
// Compile with: gcc -O2 -Wall -Wextra -std=c17 -o calc calc.c -lm
// Resources used: ChatGPT (OpenAI), for parser design and debugging.

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

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

 /*
  * A primary expression is either an integer or an expression in
  * parentheses. This parser code was developed with AI assistance.
  */
static long long parse_primary(Parser *p) {
    skip_spaces(p);

    if (p->pos >= p->end) {
        p->error_pos = p->end + 1;
        return 0;
    }

    if (p->s[p->pos] == '(') {
        p->pos++;

        long long value = parse_expression(p);
        if (p->error_pos != 0) {
            return 0;
        }

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

static long long parse_term(Parser *p) {
    long long value = parse_primary(p);
    if (p->error_pos != 0) return 0;

    while (1) {
        skip_spaces(p);

        if (p->pos >= p->end ||
            (p->s[p->pos] != '*' && p->s[p->pos] != '/')) {
            break;
        }

        char op = p->s[p->pos++];
        skip_spaces(p);
        size_t right_start = p->pos;

        long long right = parse_primary(p);
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

        if (j < end) {
            size_t line_error = 0;
            long long result = eval_line(buf, j, end, &line_error);

            if (line_error != 0) {
                error_pos = line_error;
                break;
            }

            fprintf(temp, "%lld\n", result);
        }

        i = end + 1;
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

int main(int argc, char **argv) {
    const char *outdir = NULL;
    const char *input = NULL;

    for (int a = 1; a < argc; a++) {
        if (strcmp(argv[a], "-o") == 0 && a + 1 < argc) {
            outdir = argv[++a];
        } else {
            input = argv[a];
        }
    }

    if (!outdir || !input) {
        fprintf(stderr, "usage: calc -o OUTDIR input.txt\n");
        return 1;
    }

    size_t len = 0;
    char *text = read_file(input, &len);
    if (!text) {
        fprintf(stderr, "cannot read %s\n", input);
        return 1;
    }

    if (mkdir(outdir, 0777) != 0 && errno != EEXIST) {
        fprintf(stderr, "cannot create folder %s\n", outdir);
        free(text);
        return 1;
    }

    const char *base = strrchr(input, '/');
    base = base ? base + 1 : input;

    size_t baselen = strlen(base);
    if (baselen > 4 && strcmp(base + baselen - 4, ".txt") == 0) {
        baselen -= 4;
    }

    char path[1024];
    snprintf(path, sizeof path,
             "%s/%.*s_" NAME "_" LASTNAME "_" STUDENTID ".txt",
             outdir, (int)baselen, base);

    FILE *out = fopen(path, "wb");
    if (!out) {
        fprintf(stderr, "cannot write %s\n", path);
        free(text);
        return 1;
    }

    process_text(text, len, out);
    fclose(out);
    free(text);
    return 0;
}
