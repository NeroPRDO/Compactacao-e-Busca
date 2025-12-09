/* tests/gen_story.c — gera arquivo grande repetindo a historia base. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv)
{
    if (argc < 4)
    {
        fprintf(stderr, "Uso: %s <base_story.txt> <out.txt> <megas>\n", argv[0]);
        return 2;
    }
    const char *in = argv[1];
    const char *out = argv[2];
    long long mb = atoll(argv[3]);
    if (mb <= 0)
    {
        fprintf(stderr, "megas invalido.\n");
        return 2;
    }

    FILE *fi = fopen(in, "rb");
    if (!fi)
    {
        perror("fopen base");
        return 1;
    }
    if (fseek(fi, 0, SEEK_END) != 0)
    {
        perror("fseek");
        fclose(fi);
        return 5;
    }
    long n = ftell(fi); /* base pequena; ftell de 32-bit e' suficiente aqui */
    if (n <= 0)
    {
        fprintf(stderr, "historia vazia.\n");
        fclose(fi);
        return 1;
    }
    if (fseek(fi, 0, SEEK_SET) != 0)
    {
        perror("fseek");
        fclose(fi);
        return 5;
    }

    char *buf = (char *)malloc((size_t)n);
    if (!buf)
    {
        fprintf(stderr, "sem memoria.\n");
        fclose(fi);
        return 4;
    }
    if (fread(buf, 1, (size_t)n, fi) != (size_t)n)
    {
        perror("fread");
        free(buf);
        fclose(fi);
        return 5;
    }
    fclose(fi);

    FILE *fo = fopen(out, "wb");
    if (!fo)
    {
        perror("fopen out");
        free(buf);
        return 2;
    }
    const long long target = mb * 1024ll * 1024ll;
    long long written = 0;
    while (written < target)
    {
        size_t to_write = (size_t)((target - written) > (long long)n ? (size_t)n : (size_t)(target - written));
        if (fwrite(buf, 1, to_write, fo) != to_write)
        {
            perror("fwrite");
            free(buf);
            fclose(fo);
            return 8;
        }
        written += to_write;
    }
    free(buf);
    fclose(fo);
    fprintf(stdout, "OK: %s (%lld bytes)\n", out, (long long)written);
    return 0;
}