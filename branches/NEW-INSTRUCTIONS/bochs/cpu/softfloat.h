/*============================================================================

This C header file is part of the SoftFloat IEC/IEEE Floating-point Arithmetic
Package, Release 2b.

Written by John R. Hauser.  This work was made possible in part by the
International Computer Science Institute, located at Suite 600, 1947 Center
Street, Berkeley, California 94704.  Funding was partially provided by the
National Science Foundation under grant MIP-9311980.  The original version
of this code was written as part of a project to build a fixed-point vector
processor in collaboration with the University of California at Berkeley,
overseen by Profs. Nelson Morgan and John Wawrzynek.  More information
is available through the Web page `http://www.cs.berkeley.edu/~jhauser/
arithmetic/SoftFloat.html'.

THIS SOFTWARE IS DISTRIBUTED AS IS, FOR FREE.  Although reasonable effort has
been made to avoid it, THIS SOFTWARE MAY CONTAIN FAULTS THAT WILL AT TIMES
RESULT IN INCORRECT BEHAVIOR.  USE OF THIS SOFTWARE IS RESTRICTED TO PERSONS
AND ORGANIZATIONS WHO CAN AND WILL TAKE FULL RESPONSIBILITY FOR ALL LOSSES,
COSTS, OR OTHER PROBLEMS THEY INCUR DUE TO THE SOFTWARE, AND WHO FURTHERMORE
EFFECTIVELY INDEMNIFY JOHN HAUSER AND THE INTERNATIONAL COMPUTER SCIENCE
INSTITUTE (possibly via similar legal warning) AGAINST ALL LOSSES, COSTS, OR
OTHER PROBLEMS INCURRED BY THEIR CUSTOMERS AND CLIENTS DUE TO THE SOFTWARE.

Derivative works are acceptable, even for commercial purposes, so long as
(1) the source code for the derivative work includes prominent notice that
the work is derivative, and (2) the source code includes prominent notice with
these four paragraphs for those parts of this code that are retained.

=============================================================================*/

#include <config.h>      /* generated by configure script from config.h.in */

/*----------------------------------------------------------------------------
| Software IEC/IEEE floating-point types.
*----------------------------------------------------------------------------*/
typedef Bit32u float32, Float32;
typedef Bit64u float64, Float64;

/*----------------------------------------------------------------------------
| Software IEC/IEEE floating-point underflow tininess-detection mode.
*----------------------------------------------------------------------------*/
enum {
    float_tininess_after_rounding  = 0,
    float_tininess_before_rounding = 1
};

/*----------------------------------------------------------------------------
| Software IEC/IEEE floating-point rounding mode.
*----------------------------------------------------------------------------*/
enum float_round_t {
    float_round_nearest_even = 0,
    float_round_down         = 1,
    float_round_up           = 2,
    float_round_to_zero      = 3
};

/*----------------------------------------------------------------------------
| Software IEC/IEEE floating-point exception flags.
*----------------------------------------------------------------------------*/
enum float_exception_flag_t {
    float_flag_invalid   = 0x01,
    float_flag_denormal  = 0x02,
    float_flag_divbyzero = 0x04,
    float_flag_overflow  = 0x08,
    float_flag_underflow = 0x10,
    float_flag_inexact   = 0x20,
};

/*----------------------------------------------------------------------------
| Software IEC/IEEE floating-point status structure.
*----------------------------------------------------------------------------*/
struct float_status_t {
    int float_detect_tininess;
    int float_exception_flags;
    int float_rounding_mode;
    int denormals_are_zeroes;
    int underflow_flush_to_zero;
};
typedef struct float_status_t softfloat_status_word_t;

/*----------------------------------------------------------------------------
| Returns current floating point rounding mode specified by status word
*----------------------------------------------------------------------------*/
int get_float_rounding_mode(float_status_t &status);

/*----------------------------------------------------------------------------
| Routine to raise any or all of the software IEC/IEEE floating-point
| exception flags.
*----------------------------------------------------------------------------*/
void float_raise(float_status_t &statusstatus, int);

/*----------------------------------------------------------------------------
| Software IEC/IEEE integer-to-floating-point conversion routines.
*----------------------------------------------------------------------------*/
float32 int32_to_float32(Bit32s, float_status_t &status);
float64 int32_to_float64(Bit32s);
float32 int64_to_float32(Bit64s, float_status_t &status);
float64 int64_to_float64(Bit64s, float_status_t &status);

/*----------------------------------------------------------------------------
| Software IEC/IEEE single-precision conversion routines.
*----------------------------------------------------------------------------*/
Bit32s float32_to_int32(float32, float_status_t &status);
Bit32s float32_to_int32_round_to_zero(float32, float_status_t &status);
Bit64s float32_to_int64(float32, float_status_t &status);
Bit64s float32_to_int64_round_to_zero(float32, float_status_t &status);
float64 float32_to_float64(float32, float_status_t &status);

/*----------------------------------------------------------------------------
| Software IEC/IEEE single-precision operations.
*----------------------------------------------------------------------------*/
float32 float32_round_to_int(float32, float_status_t &status);
float32 float32_add(float32, float32, float_status_t &status);
float32 float32_sub(float32, float32, float_status_t &status);
float32 float32_mul(float32, float32, float_status_t &status);
float32 float32_div(float32, float32, float_status_t &status);
float32 float32_rem(float32, float32, float_status_t &status);
float32 float32_max(float32, float32, float_status_t &status); // FixMe
float32 float32_min(float32, float32, float_status_t &status); // FixMe
float32 float32_sqrt(float32, float_status_t &status);
int float32_eq(float32, float32, float_status_t &status);
int float32_le(float32, float32, float_status_t &status);
int float32_lt(float32, float32, float_status_t &status);
int float32_eq_signaling(float32, float32, float_status_t &status);
int float32_le_quiet(float32, float32, float_status_t &status);
int float32_lt_quiet(float32, float32, float_status_t &status);
int float32_is_signaling_nan(float32);

/*----------------------------------------------------------------------------
| Software IEC/IEEE double-precision conversion routines.
*----------------------------------------------------------------------------*/
int float64_to_int32(float64, float_status_t &status);
int float64_to_int32_round_to_zero(float64, float_status_t &status);
long long float64_to_int64(float64, float_status_t &status);
long long float64_to_int64_round_to_zero(float64, float_status_t &status);
float32 float64_to_float32(float64, float_status_t &status);

/*----------------------------------------------------------------------------
| Software IEC/IEEE double-precision operations.
*----------------------------------------------------------------------------*/
float64 float64_round_to_int(float64, float_status_t &status);
float64 float64_add(float64, float64, float_status_t &status);
float64 float64_sub(float64, float64, float_status_t &status);
float64 float64_mul(float64, float64, float_status_t &status);
float64 float64_div(float64, float64, float_status_t &status);
float64 float64_rem(float64, float64, float_status_t &status);
float64 float64_max(float64, float64, float_status_t &status); // FixMe
float64 float64_min(float64, float64, float_status_t &status); // FixMe
float64 float64_sqrt(float64, float_status_t &status);
int float64_eq(float64, float64, float_status_t &status);
int float64_le(float64, float64, float_status_t &status);
int float64_lt(float64, float64, float_status_t &status);
int float64_eq_signaling(float64, float64, float_status_t &status);
int float64_le_quiet(float64, float64, float_status_t &status);
int float64_lt_quiet(float64, float64, float_status_t &status);
int float64_is_signaling_nan(float64);
