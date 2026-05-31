#ifndef SEAMLINE_H
#define SEAMLINE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _WIN32
#   ifdef SEAMLINE_BUILD_DLL
#       define SEAMLINE_API __declspec(dllexport)
#   elif defined(SEAMLINE_USE_DLL)
#       define SEAMLINE_API __declspec(dllimport)
#   else
#       define SEAMLINE_API
#   endif
#else
#   define SEAMLINE_API __attribute__((visibility("default")))
#endif

typedef struct {
    const uint8_t* data;   /* RGBA pixel buffer */
    int width;
    int height;
} SeamlineImage;

typedef struct {
    double match_threshold;  /* 0-255, default 5.0 */
    int horizontal;          /* 0 = vertical, 1 = horizontal */
    int auto_crop;           /* 1 = crop to min width on mismatch, 0 = error (default) */
} SeamlineOptions;

typedef struct {
    uint8_t* data;           /* RGBA result (free with seamline_free) */
    int width;
    int height;
    int ok;                  /* 1 = success, 0 = error */
    char error[256];
} SeamlineResult;

SEAMLINE_API SeamlineOptions seamline_default_options(void);

SEAMLINE_API SeamlineResult seamline_stitch(const SeamlineImage* images, int count,
                                            SeamlineOptions options);

SEAMLINE_API SeamlineResult seamline_stitch_files(const char** paths, int count,
                                                  SeamlineOptions options);

SEAMLINE_API int seamline_save_png(const SeamlineResult* result, const char* path);
SEAMLINE_API int seamline_save_jpg(const SeamlineResult* result, const char* path, int quality);

SEAMLINE_API void seamline_free(SeamlineResult* result);

#ifdef __cplusplus
}
#endif

#endif /* SEAMLINE_H */
