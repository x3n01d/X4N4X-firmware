/* Satellite ID catalog layout (shared by the app and gen_assets.py).
 * Block in the app's assets:  8-byte header, then fixed-size records.
 *
 *   header:  u32 magic "SAT1", u8 count, u8 record size, u16 reserved
 *   record:  char name[12] (NUL padded), u32 NORAD ID,
 *            u32 freq1, u32 freq2 (units of 10 Hz, 0 = none),
 *            u8 modulation, u8 protocol, u8 flags, u8 reserved
 * All values little-endian. Keep the #define lines plain numbers:
 * gen_assets.py reads them from this file. */
#ifndef SATID_CAT_H
#define SATID_CAT_H

#define CAT_MAGIC      0x31544153u
#define CAT_HDR_SIZE   8u
#define CAT_REC_SIZE   28u
#define CAT_NAME_LEN   12u

#define CAT_R_NORAD    12u
#define CAT_R_F1       16u
#define CAT_R_F2       20u
#define CAT_R_MOD      24u
#define CAT_R_PROTO    25u
#define CAT_R_FLAGS    26u

#define CAT_F_TEST     0x01u   /* test data: frequencies not verified */

#endif
