#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glob.h>

#include "tga.cc"

static int compare_globs(const void *a1, const void *a2)
{
    int aa1, aa2;
    aa1 = atoi(strchr(((const char **)a1)[0], '-') + 1);
    aa2 = atoi(strchr(((const char **)a2)[0], '-') + 1);
    return aa1 - aa2;
}

int main(int argc, char **argv)
{
    if (argc != 2)
        return 1;

    const char *path = argv[1];
    char pattern[1024];
    char pakname[1024];
    snprintf(pattern, sizeof(pattern), "%s/*.tga", path);
    snprintf(pakname, sizeof(pakname), "%s/out.pak", path);
    glob_t glob_results;
    glob(pattern, 0, NULL, &glob_results);
    qsort((void*)glob_results.gl_pathv, glob_results.gl_pathc,
            sizeof(char *), compare_globs);

    FILE *pak_fd = fopen(pakname, "w");
    for (int i = 0; i < glob_results.gl_pathc; ++i) {
        printf("%s\n", glob_results.gl_pathv[i]);
        int width, height;
        byte *pic = NULL;
        LoadTGA(glob_results.gl_pathv[i], &pic, &width, &height);
        if (width != 595 || height != 842) {
            giveerror("width or height are invalid\n");
        }
        if (fwrite(pic, 1, width*height, pak_fd) != width * height) {
            giveerror("write error");
        }
        free(pic);
    }


    globfree(&glob_results);
    fclose(pak_fd);

    return 0;
}
