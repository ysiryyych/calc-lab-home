// Yaroslava Sirychenko 251ADB151
// https://github.com/<ysiryyych>/calc-lab-home
// Compile with: gcc -O2 -Wall -Wextra -std=c17 -o calc calc.c -lm
// Resources used: 

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
    if (!buf) { fclose(f); return NULL; }

    size_t n;
    while ((n = fread(buf + len, 1, cap - len - 1, f)) > 0) {
        len += n;
        if (len + 1 >= cap) {            
            cap *= 2;
            char *tmp = realloc(buf, cap);
            if (!tmp) { free(buf); fclose(f); return NULL; }
            buf = tmp;
        }
    }
    fclose(f);
    buf[len] = '\0';                     
    *len_out = len;
    return buf;
}


static long long eval_line(const char *s, size_t start, size_t end,
                           size_t *error_pos) {
    size_t k = start;
    long long result = 0;
    char op = '+';
    int expect_number = 1;

    *error_pos = 0;

    while (k < end) {
        if (s[k] == ' ' || s[k] == '\t' || s[k] == '\r') {
            k++;
            continue;
        }

        if (expect_number) {
            if (!isdigit((unsigned char)s[k])) {
                *error_pos = k + 1;
                return 0;
            }

            long long num = 0;

            while (k < end && isdigit((unsigned char)s[k])) {
                num = num * 10 + (s[k] - '0');
                k++;
            }

            if (op == '+') {
                result += num;
            } else {
                result -= num;
            }

            expect_number = 0;
        } else {
            if (s[k] != '+' && s[k] != '-') {
                *error_pos = k + 1;
                return 0;
            }

            op = s[k];
            k++;
            expect_number = 1;
        }
    }

    if (expect_number) {
        *error_pos = end + 1;
        return 0;
    }

    return result;
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
    
    const char *outdir = NULL, *input = NULL;
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
    if (baselen > 4 && strcmp(base + baselen - 4, ".txt") == 0) baselen -= 4;

    char path[1024];
    snprintf(path, sizeof path, "%s/%.*s_" NAME "_" LASTNAME "_" STUDENTID ".txt",
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