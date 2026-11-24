/* math_shim_uclibc.c - FY01P uClibc libm 缺 fminf/fmaxf 的补位实现。
 *
 * molchipv500 工具链的 uClibc libm 只有 double 版 fmin/fmax, 无 C99 float 版
 * (arm-mol-linux-uclibcgnueabihf-nm sysroot/usr/lib/libm.a 实锤, 其余常用
 *  expf/logf/powf/... 均在)。refactor/src/localmapping.c 用了 fminf, PC glibc
 * 正常, ARM 链接报 undefined reference。本文件仅加入 ARM 构建 (build_arm.sh)。
 */
#include <math.h>

float fminf(float a, float b) { return a < b ? a : b; }
float fmaxf(float a, float b) { return a > b ? a : b; }
