/* 
 *  Name: Austin Liu
 *  UID: 305785051
 */

#include <stdlib.h>
#include <omp.h>

#include "utils.h"
#include "parallel.h"

/*
 * omp_get_num_threads()
 * pragma omp barrier
 * pragma omp critical
 * pragma omp atomic
 * pragma omp parallel for
 *   - reduction
 *   - private
 *   - firstprivate
*/



/*
 *  PHASE 1: compute the mean pixel value
 *  This code is buggy! Find the bug and speed it up.
 */
void mean_pixel_parallel(const uint8_t img[][NUM_CHANNELS], int num_rows, int num_cols, double mean[NUM_CHANNELS]) {
    int pixel;
    double mean0, mean1, mean2;
    const long num_pixels = num_rows * num_cols;

    #pragma omp parallel for reduction(+:mean0,mean1,mean2)
    for (pixel = 0; pixel < num_pixels; pixel++) {
        mean0 += img[pixel][0];
        mean1 += img[pixel][1];
        mean2 += img[pixel][2];
    }

    mean[0] = mean0 / num_pixels;
    mean[1] = mean1 / num_pixels;
    mean[2] = mean2 / num_pixels;
}



/*
 *  PHASE 2: convert image to grayscale and record the max grayscale value along with the number of times it appears
 *  This code is NOT buggy, just sequential. Speed it up.
 */
void grayscale_parallel(const uint8_t img[][NUM_CHANNELS], int num_rows, int num_cols, uint32_t grayscale_img[][NUM_CHANNELS], uint8_t *max_gray, uint32_t *max_count) {
    int pixel, gray, _max_gray, _max_count;
    // int counts[255] = {0};
    // omp_lock_t count_locks[255];
    const long num_pixels = num_rows * num_cols;

    // #pragma omp parallel for
    // for (int i = 0; i < 255; i++)
    //     omp_init_lock(&count_locks[i]);

    #pragma omp parallel for private(gray) reduction(max:_max_gray)
    for (pixel = 0; pixel < num_pixels; pixel++) {
        gray = (img[pixel][0] + img[pixel][1] + img[pixel][2]) / NUM_CHANNELS;

        grayscale_img[pixel][0] = gray;
        grayscale_img[pixel][1] = gray;
        grayscale_img[pixel][2] = gray;

        if (gray > _max_gray)
            _max_gray = gray;

        // if (gray == _max_gray) {
        //     omp_set_lock(&count_locks[_max_gray]);
        //     counts[_max_gray] += 3;
        //     omp_unset_lock(&count_locks[_max_gray]);
        // }
    }

    // #pragma omp parallel for
    // for (int i = 0; i < 255; i++)
    //     omp_destroy_lock(&count_locks[i]);

    #pragma omp parallel for reduction(+:_max_count)
    for (pixel = 0; pixel < num_pixels; pixel++) {
        if (grayscale_img[pixel][0] == _max_gray)
            _max_count += 3;
    }

    *max_gray = _max_gray;
    *max_count = _max_count;
}



/*
 *  PHASE 3: perform convolution on image
 *  This code is NOT buggy, just sequential. Speed it up.
 */
void convolution_parallel(const uint8_t padded_img[][NUM_CHANNELS], int num_rows, int num_cols, const uint32_t kernel[], int kernel_size, uint32_t convolved_img[][NUM_CHANNELS]) {
    int row, col, pixel, kernel_row, kernel_col, row_offset, pad_row_offset, pad_col, kernel_row_offset, pad_pixel, multiplier;
    int kernel_norm, i;
    int kernel_block, conv_rows, conv_cols;

    kernel_block = kernel_size*kernel_size;
    // compute kernel normalization factor
    #pragma omp parallel for reduction(+:kernel_norm)
    for(i = 0; i < kernel_block; i++) {
        kernel_norm += kernel[i];
    }

    // compute dimensions of convolved image
    conv_rows = num_rows - kernel_size + 1;
    conv_cols = num_cols - kernel_size + 1;

    // perform convolution
    #pragma omp parallel for private(col, pixel, kernel_row, kernel_col, row_offset, pad_row_offset, pad_col, kernel_row_offset, pad_pixel, multiplier)
    for (row = 0; row < conv_rows; row++) {
        // if (row == 0) printf("num_threads: %d\n", omp_get_num_threads());
        row_offset = row * conv_cols;
        for (col = 0; col < conv_cols; col++) {
            pixel = row_offset + col;
            convolved_img[pixel][0] = 0;
            convolved_img[pixel][1] = 0;
            convolved_img[pixel][2] = 0;

            for (kernel_row = 0; kernel_row < kernel_size; kernel_row++) {
                pad_row_offset = (row+kernel_row)*num_cols;
                kernel_row_offset = kernel_row * kernel_size;

                for (kernel_col = 0; kernel_col < kernel_size; kernel_col++) {
                    pad_pixel = pad_row_offset + col+kernel_col;
                    multiplier = kernel[kernel_row_offset + kernel_col];
                    convolved_img[pixel][0] += padded_img[pad_pixel][0] * multiplier;
                    convolved_img[pixel][1] += padded_img[pad_pixel][1] * multiplier;
                    convolved_img[pixel][2] += padded_img[pad_pixel][2] * multiplier;
                }
            }    
            convolved_img[pixel][0] /= kernel_norm;
            convolved_img[pixel][1] /= kernel_norm;
            convolved_img[pixel][2] /= kernel_norm;
        }
    }
}
