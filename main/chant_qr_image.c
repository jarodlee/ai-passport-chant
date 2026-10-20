#include "chant_qr_image.h"
#if __has_include("chant_qr_local.h")
#include "chant_qr_local.h"
#else
#include "../assets/images/chant-qr.h"
#endif
_Static_assert(CHANT_QR_WIDTH <= 224 && CHANT_QR_WIDTH == CHANT_QR_HEIGHT,
    "QR must fit the dedicated display area");
_Static_assert(sizeof(chant_qr_pixels) == 8 + CHANT_QR_STRIDE * CHANT_QR_HEIGHT,
    "Indexed QR bitmap length mismatch");
static const lv_image_dsc_t image={
    .header={.magic=LV_IMAGE_HEADER_MAGIC,.cf=LV_COLOR_FORMAT_I1,
        .w=CHANT_QR_WIDTH,.h=CHANT_QR_HEIGHT,.stride=CHANT_QR_STRIDE},
    .data_size=sizeof(chant_qr_pixels),.data=chant_qr_pixels
};
const lv_image_dsc_t *chant_qr_image(void) { return &image; }
