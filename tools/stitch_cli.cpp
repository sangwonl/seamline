#include "seamline.h"
#include <cstdio>
#include <cstring>

int main(int argc, char* argv[]) {
    if (argc < 4) {
        fprintf(stderr, "Usage: %s <output.png> <image1> <image2> [image3 ...]\n", argv[0]);
        fprintf(stderr, "  Options: --horizontal  (stitch left-to-right)\n");
        fprintf(stderr, "           --auto-crop   (crop to min width on mismatch)\n");
        return 1;
    }

    const char* output_path = argv[1];
    SeamlineOptions opts = seamline_default_options();

    // Collect input paths, check for flags
    const char* paths[256];
    int count = 0;
    for (int i = 2; i < argc && count < 256; i++) {
        if (strcmp(argv[i], "--horizontal") == 0) {
            opts.horizontal = 1;
        } else if (strcmp(argv[i], "--auto-crop") == 0) {
            opts.auto_crop = 1;
        } else {
            paths[count++] = argv[i];
        }
    }

    if (count < 2) {
        fprintf(stderr, "Error: need at least 2 input images\n");
        return 1;
    }

    printf("Stitching %d images...\n", count);
    SeamlineResult result = seamline_stitch_files(paths, count, opts);

    if (!result.ok) {
        fprintf(stderr, "Error: %s\n", result.error);
        return 1;
    }

    printf("Result: %dx%d\n", result.width, result.height);

    int saved = seamline_save_png(&result, output_path);
    seamline_free(&result);

    if (!saved) {
        fprintf(stderr, "Failed to save: %s\n", output_path);
        return 1;
    }

    printf("Saved: %s\n", output_path);
    return 0;
}
