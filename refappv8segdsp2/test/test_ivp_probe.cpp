/*
 * test_ivp_probe.cpp - empirical semantics of the cstub IVP intrinsics.
 *
 * The accelerated VP6+IVP kernels (fast9_scoremap_vp6, orb_blur_kernel_ivp,
 * brief/ic_angle IVP paths) rest on precise lane/rounding semantics of a
 * handful of IVP intrinsics. The Cadence headers here are auto-generated
 * cstub wrappers without doc comments, so this probe pins each assumption
 * empirically: build vector(s) from a known buffer, run the op, store the
 * result back, and compare against the scalar expectation.
 *
 * Every line the kernels depend on must PASS. Run: test_ivp_probe.exe
 * (exit 0 = all pass).
 */
#include "xtensa/tie/Xm_fy01_202210.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifndef ALIGN64
#  ifdef _MSC_VER
#    define ALIGN64 __declspec(align(64))
#  else
#    define ALIGN64 __attribute__((aligned(64)))
#  endif
#endif

static int g_fails = 0;
static void chk(int ok, const char *what)
{
    printf("%-64s %s\n", what, ok ? "PASS" : "FAIL");
    if (!ok) ++g_fails;
}

/* ---- helpers: all vector inspection goes through the load/store ops ---- */
static ALIGN64 uint8_t g_in[256];     /* distinct bytes: i&0xFF                    */
static ALIGN64 uint8_t g_pool[256];   /* constant pool                             */
static ALIGN64 uint8_t g_out[512];    /* prefilled with 0xEE sentinel              */

static void fill_in(void)
{
    for (int i = 0; i < 256; ++i) g_in[i] = (uint8_t)i;
    for (int i = 0; i < 256; ++i) g_pool[i] = 0xA5;
    for (int i = 0; i < 512; ++i) g_out[i] = 0xEE;
}

static xb_vec2Nx8U ld2N(const uint8_t *p)   /* plain load at byte pointer */
{
    return IVP_LV2NX8U_X((const xb_vec2Nx8U *)(const void *)p, 0);
}

static int st2N(xb_vec2Nx8U v, uint8_t *p)  /* store, return bytes written */
{
    xb_vec2Nx8U *q = (xb_vec2Nx8U *)(void *)p;
    IVP_SV2NX8U_IP(v, q, (int)sizeof(v));
    return (int)sizeof(v);
}

/* number of non-sentinel bytes in g_out */
static int out_used(void)
{
    int n = 0;
    for (int i = 0; i < 512; ++i) if (g_out[i] != 0xEE) ++n;
    return n;
}

int main(void)
{
    fill_in();

    /* ---------- T1: SEQ2NX8U lane-index sequence ---------- */
    {
        for (int i = 0; i < 512; ++i) g_out[i] = 0xEE;
        xb_vec2Nx8U s = IVP_SEQ2NX8U();
        st2N(s, g_out);
        int n = out_used();
        int ok = (n > 0);
        for (int i = 0; i < n && i < 512; ++i) if (g_out[i] != (uint8_t)i) ok = 0;
        printf("T1 SEQ2NX8U: %d lanes, first=%d,%d,%d\n", n, g_out[0], g_out[1], g_out[2]);
        chk(ok, "T1 IVP_SEQ2NX8U = lane index bytes 0..L-1");
    }

    /* ---------- T2: LV2NX8U_X offset unit + lane count ---------- */
    {
        for (int i = 0; i < 512; ++i) g_out[i] = 0xEE;
        xb_vec2Nx8U v = IVP_LV2NX8U_X((const xb_vec2Nx8U *)(const void *)g_in, 3);
        st2N(v, g_out);
        int n = out_used();
        int ok = 1;
        for (int i = 0; i < n; ++i) if (g_out[i] != g_in[3 + i]) ok = 0;
        printf("T2 LV_X(off=3): %d lanes, out[0]=%d (in[3]=%d)\n", n, g_out[0], g_in[3]);
        chk(ok, "T2 IVP_LV2NX8U_X(p,c) reads bytes p[c .. c+L-1]");
    }

    /* ---------- T3: SUB2NX8U saturate vs wrap ---------- */
    {
        ALIGN64 uint8_t A[64], B[64];
        memset(A, 10, sizeof A); memset(B, 20, sizeof B);
        xb_vec2Nx8U r = IVP_SUB2NX8U(ld2N(A), ld2N(B));
        for (int i = 0; i < 512; ++i) g_out[i] = 0xEE;
        st2N(r, g_out);
        printf("T3 SUB(10,20) -> %d\n", g_out[0]);
        chk(g_out[0] == 246, "T3 IVP_SUB2NX8U wraps mod 256 (10-20 = 246, no sat)");
        r = IVP_SUB2NX8U(ld2N(B), ld2N(A));
        st2N(r, g_out);
        chk(g_out[0] == 10, "T3b IVP_SUB2NX8U(20,10) = 10");
    }

    /* ---------- T4: ADD2NX8U wrap vs saturate ---------- */
    {
        ALIGN64 uint8_t A[64], B[64];
        memset(A, 200, sizeof A); memset(B, 100, sizeof B);
        xb_vec2Nx8U r = IVP_ADD2NX8U(ld2N(A), ld2N(B));
        for (int i = 0; i < 512; ++i) g_out[i] = 0xEE;
        st2N(r, g_out);
        printf("T4 ADD(200,100) -> %d\n", g_out[0]);
        chk(g_out[0] == 44, "T4 IVP_ADD2NX8U wraps mod 256 (200+100 = 44, no sat)");
    }

    /* ---------- T5: LTU2NX8 + MOV2NX8UT select + arg order ---------- */
    {
        ALIGN64 uint8_t A[64], B[64], X[64], Y[64];
        memset(A, 5, sizeof A); memset(B, 9, sizeof B);
        memset(X, 0xCC, sizeof X); memset(Y, 0x33, sizeof Y);
        vbool2N lt = IVP_LTU2NX8(ld2N(A), ld2N(B));   /* 5 < 9 -> true */
        xb_vec2Nx8U r = IVP_MOV2NX8UT(ld2N(X), ld2N(Y), lt);
        for (int i = 0; i < 512; ++i) g_out[i] = 0xEE;
        st2N(r, g_out);
        chk(g_out[0] == 0xCC, "T5a MOVUT(b,c,d) = d ? b : c  (LTU(5,9)=true)");
        vbool2N ge = IVP_LTU2NX8(ld2N(B), ld2N(A));   /* 9 < 5 -> false */
        r = IVP_MOV2NX8UT(ld2N(X), ld2N(Y), ge);
        st2N(r, g_out);
        chk(g_out[0] == 0x33, "T5b LTU(9,5)=false selects c");
    }

    /* ---------- T6: MINU/MAXU elementwise ---------- */
    {
        ALIGN64 uint8_t A[64], B[64];
        for (int i = 0; i < 64; ++i) { A[i] = (uint8_t)(i * 4); B[i] = (uint8_t)(255 - i); }
        xb_vec2Nx8U mn = IVP_MINU2NX8(ld2N(A), ld2N(B));
        xb_vec2Nx8U mx = IVP_MAXU2NX8(ld2N(A), ld2N(B));
        for (int i = 0; i < 512; ++i) g_out[i] = 0xEE;
        st2N(mn, g_out);
        int ok = 1;
        for (int i = 0; i < 64; ++i) if (g_out[i] != (A[i] < B[i] ? A[i] : B[i])) ok = 0;
        st2N(mx, g_out);
        for (int i = 0; i < 64; ++i) if (g_out[i] != (A[i] > B[i] ? A[i] : B[i])) ok = 0;
        chk(ok, "T6 MINU/MAXU elementwise per lane");
    }

    /* ---------- T7: MULUS2N8XR16 + CVT16U2NX24L/H lane mapping ---------- */
    {
        ALIGN64 uint8_t A[64];
        for (int i = 0; i < 64; ++i) A[i] = (uint8_t)(i + 1);   /* 1..64 */
        xb_vec2Nx24 p24 = IVP_MULUS2N8XR16(ld2N(A), 7);
        xb_vecNx16 lo16 = IVP_CVT16U2NX24L(p24);
        xb_vecNx16 hi16 = IVP_CVT16U2NX24H(p24);
        ALIGN64 uint16_t L[32], H[32];
        memset(L, 0xEE, sizeof L); memset(H, 0xEE, sizeof H);
        xb_vecNx16U *pl = (xb_vecNx16U *)(void *)L, *ph = (xb_vecNx16U *)(void *)H;
        IVP_SVNX16U_IP(*(xb_vecNx16U *)&lo16, pl, (int)sizeof(lo16));
        IVP_SVNX16U_IP(*(xb_vecNx16U *)&hi16, ph, (int)sizeof(hi16));
        int nL = 0, nH = 0;
        while (nL < 32 && L[nL] != 0xEEEE) ++nL;
        while (nH < 32 && H[nH] != 0xEEEE) ++nH;
        printf("T7 MULUS lanes: L=[%d,%d,%d..%d](n=%d) H=[%d,%d..%d](n=%d)\n",
               L[0], L[1], L[2], L[nL > 0 ? nL - 1 : 0], nL,
               H[0], H[1], H[nH > 0 ? nH - 1 : 0], nH);
        int okL = 1, okH = 1;
        for (int i = 0; i < nL; ++i) if (L[i] != (uint16_t)((i + 1) * 7)) okL = 0;
        for (int i = 0; i < nH; ++i) if (H[i] != (uint16_t)((i + 1 + 32) * 7)) okH = 0;
        printf("   CVT..L = first half (in lanes 0-31)? %s ; CVT..H = second half? %s\n",
               okL ? "yes" : "NO", okH ? "yes" : "NO");
        chk(okL && okH && nL + nH == 64, "T7 MULUS(v,s)*7 lanes, CVT L/H = lane halves");
    }

    /* ---------- T8: MULUSPA2N8XR16 accumulate semantics ---------- */
    {
        ALIGN64 uint8_t A[64], B[64];
        for (int i = 0; i < 64; ++i) { A[i] = (uint8_t)(i + 1); B[i] = (uint8_t)(100 + i); }
        xb_vec2Nx24 acc = IVP_MULUS2N8XR16(ld2N(A), 1);       /* acc = A */
        IVP_MULUSPA2N8XR16(acc, ld2N(B), ld2N(B), 7);         /* acc += ? */
        xb_vecNx16 lo16 = IVP_CVT16U2NX24L(acc);
        ALIGN64 uint16_t L[32];
        memset(L, 0xEE, sizeof L);
        xb_vecNx16U *pl = (xb_vecNx16U *)(void *)L;
        IVP_SVNX16U_IP(*(xb_vecNx16U *)&lo16, pl, (int)sizeof(lo16));
        printf("T8 MULUSPA(acc=A, B, B, 7): L[0]=%d (A=1): A+7B=%d, A+B*7=%d, A+B=%d\n",
               L[0], 1 + 7 * 100, 1 + 100 * 7, 1 + 100);
        chk(L[0] == (uint16_t)(1 + 7 * 100), "T8 MULUSPA2N8XR16(a,b,c,d): a += b*d");
    }

    /* ---------- T9: MULUSPAN16XR16 (Nx16 accumulate) ---------- */
    {
        ALIGN64 uint16_t W[32];
        for (int i = 0; i < 32; ++i) W[i] = (uint16_t)(1000 + i);
        const xb_vecNx16U *pw = (const xb_vecNx16U *)(const void *)W;
        xb_vecNx16U w = IVP_LVNX16U_X(pw, 0);
        xb_vecNx48 a48; memset(&a48, 0, sizeof a48);
        IVP_MULUSPAN16XR16(a48, w, w, 3);
        xb_vecNx16 lo = IVP_PACKLNX48(a48);
        ALIGN64 uint16_t L[32];
        memset(L, 0xEE, sizeof L);
        xb_vecNx16U *pl = (xb_vecNx16U *)(void *)L;
        IVP_SVNX16U_IP(*(xb_vecNx16U *)&lo, pl, (int)sizeof(lo));
        int n = 0; while (n < 32 && L[n] != 0xEEEE) ++n;
        printf("T9 MULUSPAN16(a0, w, w, 3): n=%d L[0]=%d (expect 3000 = 1000*3)\n", n, L[0]);
        chk(L[0] == 3000 && n >= 16, "T9 MULUSPAN16XR16(a,b,c,d): a += b*d (16 lanes)");
    }

    /* ---------- T10: PACKVRU2NX24 trunc vs round ---------- */
    {
        ALIGN64 uint8_t P128[64], P192[64];
        memset(P128, 128, sizeof P128); memset(P192, 192, sizeof P192);
        /* 128*256 = 32768: trunc>>16 = 0, round = (32768+32768)>>16 = 1 */
        xb_vec2Nx24 a = IVP_MULUS2N8XR16(ld2N(P128), 256);
        xb_vec2Nx8U r0 = IVP_PACKVRU2NX24(a, 16);
        /* 192*512 = 98304: trunc = 1, round = (98304+32768)>>16 = 2 */
        xb_vec2Nx24 b = IVP_MULUS2N8XR16(ld2N(P192), 512);
        xb_vec2Nx8U r1 = IVP_PACKVRU2NX24(b, 16);
        for (int i = 0; i < 512; ++i) g_out[i] = 0xEE;
        st2N(r0, g_out);
        int v0 = g_out[0];
        st2N(r1, g_out);
        int v1 = g_out[0];
        printf("T10 PACKVRU(32768,16)=%d (0=trunc,1=round); PACKVRU(98304,16)=%d (1=trunc,2=round)\n", v0, v1);
        chk(v0 == 0 ? v1 == 1 : v1 == 2, "T10 PACKVRU2NX24 shift amount consistent");
    }

    /* ---------- T11: UNPKU2NX8_0/1 even/odd lanes ---------- */
    {
        xb_vec2Nx8U s = IVP_SEQ2NX8U();
        xb_vecNx16 e = IVP_UNPKU2NX8_0(s);
        xb_vecNx16 o = IVP_UNPKU2NX8_1(s);
        ALIGN64 uint16_t E[32], O[32];
        memset(E, 0xEE, sizeof E); memset(O, 0xEE, sizeof O);
        xb_vecNx16U *pe = (xb_vecNx16U *)(void *)E, *po = (xb_vecNx16U *)(void *)O;
        IVP_SVNX16U_IP(*(xb_vecNx16U *)&e, pe, (int)sizeof(e));
        IVP_SVNX16U_IP(*(xb_vecNx16U *)&o, po, (int)sizeof(o));
        printf("T11 UNPK_0: E=[%d,%d,%d..] UNPK_1: O=[%d,%d,%d..]\n", E[0], E[1], E[2], O[0], O[1], O[2]);
        int ok = E[0] == 0 && E[1] == 2 && O[0] == 1 && O[1] == 3;
        chk(ok, "T11 UNPKU2NX8_0 = even bytes, _1 = odd bytes");
    }

    /* ---------- T12: GATHER bytes ---------- */
    {
        ALIGN64 uint16_t offs[32];
        for (int i = 0; i < 32; ++i) offs[i] = (uint16_t)(i * 4);   /* byte offsets? */
        const xb_vecNx16U *po = (const xb_vecNx16U *)(const void *)offs;
        xb_vecNx16U ov = IVP_LVNX16U_X(po, 0);
        xb_gsr g = IVP_GATHERANX8U(g_in, ov);
        xb_vecNx16U got = IVP_GATHERDNX8U(g);
        ALIGN64 uint16_t G[32];
        memset(G, 0xEE, sizeof G);
        xb_vecNx16U *pg = (xb_vecNx16U *)(void *)G;
        IVP_SVNX16U_IP(got, pg, (int)sizeof(got));
        printf("T12 GATHER offs=i*4: G=[%d,%d,%d,%d..] (in[0/4/8/12]=%d,%d,%d,%d)\n",
               G[0], G[1], G[2], G[3], g_in[0], g_in[4], g_in[8], g_in[12]);
        chk(G[0] == 0 && G[1] == 4 && G[2] == 8 && G[3] == 12,
            "T12 GATHERANX8U+GATHERDNX8U: bytes at base+off[i], 16 lanes");
    }

    /* ---------- T13: LA/LAV streaming unaligned loads ---------- */
    {
        const xb_vec2Nx8U *p = (const xb_vec2Nx8U *)(const void *)(g_in + 5);
        valign va = IVP_LA2NX8U_PP(p);
        xb_vec2Nx8U v1, v2;
        IVP_LAV2NX8U_XP(v1, va, p, (int)sizeof(v1));
        IVP_LAV2NX8U_XP(v2, va, p, (int)sizeof(v2));
        for (int i = 0; i < 512; ++i) g_out[i] = 0xEE;
        st2N(v1, g_out);
        int a0 = g_out[0], a1 = g_out[1];
        st2N(v2, g_out);
        int b0 = g_out[0], b1 = g_out[1];
        printf("T13 LAV: v1[0,1]=%d,%d v2[0,1]=%d,%d (in[5,6]=%d,%d in[69,70]=%d,%d)\n",
               a0, a1, b0, b1, g_in[5], g_in[6], g_in[69], g_in[70]);
        chk(a0 == 5 && a1 == 6 && b0 == 69 && b1 == 70,
            "T13 LA2NX8U_PP + LAV2NX8U_XP stream bytes from unaligned p");
    }

    /* ---------- T14: SA/SAV/SAPOS streaming unaligned stores ---------- */
    {
        for (int i = 0; i < 512; ++i) g_out[i] = 0xEE;
        xb_vec2Nx8U *p = (xb_vec2Nx8U *)(void *)(g_out + 7);
        valign sa;
        IVP_SA2NX8U_IP(ld2N(g_in + 0), sa, p);
        IVP_SA2NX8U_IP(ld2N(g_in + 64), sa, p);
        IVP_SA2NX8U_IP(ld2N(g_in + 128), sa, p);
        IVP_SAPOS2NX8U_FP(sa, p);
        int n = out_used();
        int ok = n == 192;
        for (int i = 0; i < 192 && ok; ++i) if (g_out[7 + i] != g_in[i]) ok = 0;
        printf("T14 SA/SAPOS: %d bytes written at dst+7\n", n);
        chk(ok, "T14 SA2NX8U_IP x3 + SAPOS writes all 192 bytes at unaligned dst");
    }

    /* ---------- T15: CVT24U2NX16 halves ---------- */
    {
        ALIGN64 uint16_t A[32], B[32];
        for (int i = 0; i < 32; ++i) { A[i] = (uint16_t)(100 + i); B[i] = (uint16_t)(200 + i); }
        const xb_vecNx16U *pa = (const xb_vecNx16U *)(const void *)A;
        const xb_vecNx16U *pb = (const xb_vecNx16U *)(const void *)B;
        xb_vecNx16U va = IVP_LVNX16U_X(pa, 0);
        xb_vecNx16U vb = IVP_LVNX16U_X(pb, 0);
        xb_vec2Nx24 c24 = IVP_CVT24U2NX16(*(const xb_vecNx16 *)&va, *(const xb_vecNx16 *)&vb);
        xb_vecNx16 lo16 = IVP_CVT16U2NX24L(c24);
        ALIGN64 uint16_t L[32];
        memset(L, 0xEE, sizeof L);
        xb_vecNx16U *pl = (xb_vecNx16U *)(void *)L;
        IVP_SVNX16U_IP(*(xb_vecNx16U *)&lo16, pl, (int)sizeof(lo16));
        printf("T15 CVT24U2NX16(a,b) L=[%d,%d..] (a=[100,101..] b=[200,201..])\n", L[0], L[1]);
        chk(L[0] == 200 && L[1] == 201, "T15 CVT24U2NX16(a,b): SECOND arg -> low (L) half");
    }

    /* ---------- T16: byte-plane deinterleave chain (blur vertical pass) ------
     * tmp u16 bytes X[2i]=lo_i, X[2i+1]=hi_i. The chain
     *   PACKL2NX24(CVT24U2NX16(UNPK_1(vB), UNPK_1(vA)))
     * must yield the 64 hi-plane bytes in lane order [hiA(32) | hiB(32)]. ---- */
    {
        ALIGN64 uint8_t X[128];
        for (int i = 0; i < 128; ++i) X[i] = (uint8_t)(i + 1);
        xb_vec2Nx8U vA = IVP_LV2NX8U_X((const xb_vec2Nx8U *)(const void *)X, 0);
        xb_vec2Nx8U vB = IVP_LV2NX8U_X((const xb_vec2Nx8U *)(const void *)X, 64);
        xb_vec2Nx24 h24 = IVP_CVT24U2NX16(IVP_UNPKU2NX8_1(*(const xb_vec2Nx8 *)&vB),
                                          IVP_UNPKU2NX8_1(*(const xb_vec2Nx8 *)&vA));
        xb_vec2Nx8 hi8s = IVP_PACKL2NX24(h24);
        xb_vec2Nx24 l24 = IVP_CVT24U2NX16(IVP_UNPKU2NX8_0(*(const xb_vec2Nx8 *)&vB),
                                          IVP_UNPKU2NX8_0(*(const xb_vec2Nx8 *)&vA));
        xb_vec2Nx8 lo8s = IVP_PACKL2NX24(l24);
        xb_vec2Nx8U hi8 = *(const xb_vec2Nx8U *)(const void *)&hi8s;
        xb_vec2Nx8U lo8 = *(const xb_vec2Nx8U *)(const void *)&lo8s;
        for (int i = 0; i < 512; ++i) g_out[i] = 0xEE;
        st2N(hi8, g_out);
        int ok = 1;
        for (int i = 0; i < 64; ++i) if (g_out[i] != X[2 * i + 1]) ok = 0;
        st2N(lo8, g_out);
        for (int i = 0; i < 64; ++i) if (g_out[i] != X[2 * i]) ok = 0;
        printf("T16 plane chain: hi[0..3]=%d,%d,%d,%d lo[0..3]=%d,%d,%d,%d (want 2,4,6,8 / 1,3,5,7)\n",
               g_out[0], g_out[1], g_out[2], g_out[3], g_out[0], g_out[1], g_out[2], g_out[3]);
        chk(ok, "T16 PACKL(CVT24(UNPK_1(vB),UNPK_1(vA))) = hi plane [A|B] in order");
    }

    /* ---------- T17: many interleaved LA/LAV streams (scoremap 17-ring load
     * pattern): each stream independently delivers [start+64*i, +64) per
     * LAV, regardless of the other streams' alignments. ---------- */
    {
        ALIGN64 uint8_t R[8][320];            /* 8 rows x 320 bytes */
        for (int r = 0; r < 8; ++r)
            for (int i = 0; i < 320; ++i) R[r][i] = (uint8_t)(r * 41 + i);
        /* 17 streams like the kernel: 16 ring (row, dx) + 1 centre */
        const int rows[17] = {0,0,1,2,3,4,5,6,6,6,5,4,3,2,1,0,3};
        const int dxs [17] = {0,1,2,3,3,3,2,1,0,-1,-2,-3,-3,-3,-2,-1,0};
        const int start = 5;                  /* unaligned stream base */
        const xb_vec2Nx8U *lp[17];
        valign lv[17];
        for (int s = 0; s < 17; ++s) {
            lp[s] = (const xb_vec2Nx8U *)(const void *)(R[rows[s]] + start + dxs[s]);
            lv[s] = IVP_LA2NX8U_PP(lp[s]);
        }
        int ok = 1;
        for (int chunk = 0; chunk < 4; ++chunk) {          /* 4 chunks */
            for (int s = 0; s < 17; ++s) {
                xb_vec2Nx8U v;
                IVP_LAV2NX8U_XP(v, lv[s], lp[s], 64);
                const uint8_t *truth = R[rows[s]] + start + dxs[s] + chunk * 64;
                for (int i = 0; i < 64; ++i) {
                    uint8_t got = ((const uint8_t *)&v)[i];
                    if (got != truth[i]) { ok = 0; }
                }
            }
        }
        chk(ok, "T17 17 interleaved LA/LAV streams keep per-stream offsets");
    }

    /* ---------- T18: clamped-lane FAST-9 tree vs brute-force arc scan ------
     * The kernel computes, per lane: u=max(ring-p,0), e=max(p-ring,0),
     * S'=max(tree(u),tree(e)) with the shared-core tree, out = S'>t ? S'-1
     * : 0xFF. Brute force: 16 arcs x 9 pixels, both polarities, raw ints. */
    {
        /* deterministic pseudo-random bytes */
        ALIGN64 uint8_t ring[17][64];
        unsigned rs = 0x1234u;
        for (int i = 0; i < 17; ++i) for (int j = 0; j < 64; ++j) {
            rs = rs * 1103515245u + 12345u;
            ring[i][j] = (uint8_t)(rs >> 16);
        }
        const int t_floor = 7;
        /* kernel-form vectors */
        xb_vec2Nx8U vc = ld2N(ring[16]);
        ALIGN64 uint8_t tfb[64], ntb[64];
        memset(tfb, t_floor, 64); memset(ntb, 0xFF, 64);
        xb_vec2Nx8U vtf = ld2N(tfb), vnone = ld2N(ntb);
        xb_vec2Nx8U vzero = IVP_SUB2NX8U(vtf, vtf);
        xb_vec2Nx8U u[16], e[16];
        for (int i = 0; i < 16; ++i) {
            xb_vec2Nx8U vi = ld2N(ring[i]);
            u[i] = IVP_MOV2NX8UT(IVP_SUB2NX8U(vi, vc), vzero, IVP_LTU2NX8(vc, vi));
            e[i] = IVP_MOV2NX8UT(IVP_SUB2NX8U(vc, vi), vzero, IVP_LTU2NX8(vi, vc));
        }
        /* shared-core tree (same expression as fs_tree_ivp) */
        xb_vec2Nx8U m2[22], m4[20], core[16];
        for (int j = 0; j < 22; ++j) m2[j] = IVP_MINU2NX8(u[j & 15], u[(j + 1) & 15]);
        for (int j = 0; j < 20; ++j) m4[j] = IVP_MINU2NX8(m2[j], m2[j + 2]);
        for (int j = 0; j < 16; ++j) core[j] = IVP_MINU2NX8(m4[j], m4[j + 4]);
        xb_vec2Nx8U Sb = IVP_MINU2NX8(core[1], IVP_MAXU2NX8(u[0], u[9]));
        for (int s = 2; s < 16; s += 2)
            Sb = IVP_MAXU2NX8(Sb, IVP_MINU2NX8(core[s + 1], IVP_MAXU2NX8(u[s], u[(s + 9) & 15])));
        for (int j = 0; j < 22; ++j) m2[j] = IVP_MINU2NX8(e[j & 15], e[(j + 1) & 15]);
        for (int j = 0; j < 20; ++j) m4[j] = IVP_MINU2NX8(m2[j], m2[j + 2]);
        for (int j = 0; j < 16; ++j) core[j] = IVP_MINU2NX8(m4[j], m4[j + 4]);
        xb_vec2Nx8U Se = IVP_MINU2NX8(core[1], IVP_MAXU2NX8(e[0], e[9]));
        for (int s = 2; s < 16; s += 2)
            Se = IVP_MAXU2NX8(Se, IVP_MINU2NX8(core[s + 1], IVP_MAXU2NX8(e[s], e[(s + 9) & 15])));
        xb_vec2Nx8U Sp = IVP_MAXU2NX8(Sb, Se);
        xb_vec2Nx8U out = IVP_MOV2NX8UT(IVP_ADD2NX8U(Sp, vnone), vnone, IVP_LTU2NX8(vtf, Sp));
        for (int i = 0; i < 512; ++i) g_out[i] = 0xEE;
        st2N(out, g_out);
        /* brute force per lane */
        int ok = 1;
        for (int lane = 0; lane < 64; ++lane) {
            int p = ring[16][lane], d[16];
            for (int i = 0; i < 16; ++i) d[i] = (int)ring[i][lane] - p;
            int S = -1 << 20;
            for (int pol = 0; pol < 2; ++pol) {
                const int *q = pol ? d : d;               /* bright then dark */
                int dd[16];
                for (int i = 0; i < 16; ++i) dd[i] = pol ? -d[i] : d[i];
                for (int s = 0; s < 16; ++s) {
                    int m = 255;
                    for (int k = 0; k < 9; ++k) { int v = dd[(s + k) & 15]; if (v < m) m = v; }
                    if (m > S) S = m;
                }
            }
            uint8_t want = (S > t_floor) ? (uint8_t)(S - 1) : 0xFF;
            if (g_out[lane] != want) { ok = 0; printf("   T18 lane %d: got %d want %d (S=%d)\n", lane, g_out[lane], want, S); }
        }
        chk(ok, "T18 clamped-lane tree == brute-force 16-arc scan + emit");
    }

    /* ---------- T19: blur vertical chunk (split hi/lo-plane MACs) ----------
     * tmp u16 rows read as byte pairs; chain per 64 columns:
     *   vA/vB = 128 raw bytes, hi/lo planes via the T16 chain,
     *   accH = sum g_k*hi_k, accL = sum g_k*lo_k (each <= 256*255 < 2^23),
     *   L1 = PACKVRNR(accL, 8) = L >> 8, accH += L1 (MULUSPA .., 1),
     *   out = PACKVRU(accH, 8) = (H + (L>>8) + 128) >> 8
     * must equal (sum_k g_k*tmp_k + 32768) >> 16 per column:
     *   sum g*t = 256H + L  =>  (256H + L + 32768) >> 16 = (H + (L>>8) + 128) >> 8
     * exactly (see T20 for why the single-accumulator form is unusable). */
    {
        const int G[7] = { 18, 34, 48, 56, 48, 34, 18 };
        ALIGN64 uint16_t t7[7][64];
        unsigned rs = 0x9e37u;
        for (int k = 0; k < 7; ++k) for (int i = 0; i < 64; ++i) {
            rs = rs * 1103515245u + 12345u;
            t7[k][i] = (uint16_t)((rs >> 16) & 0xFFFF);   /* full u16 range */
        }
        const uint8_t *tr[7];
        for (int k = 0; k < 7; ++k) tr[k] = (const uint8_t *)t7[k];
        int x = 0;

        xb_vec2Nx24 accH, accL;
        for (int k = 0; k < 7; ++k) {
            xb_vec2Nx8U vA = IVP_LV2NX8U_X((const xb_vec2Nx8U *)(const void *)(tr[k] + 2 * x), 0);
            xb_vec2Nx8U vB = IVP_LV2NX8U_X((const xb_vec2Nx8U *)(const void *)(tr[k] + 2 * x + 64), 0);
            xb_vec2Nx8 hiS = IVP_PACKL2NX24(IVP_CVT24U2NX16(
                IVP_UNPKU2NX8_1(*(const xb_vec2Nx8 *)&vB),
                IVP_UNPKU2NX8_1(*(const xb_vec2Nx8 *)&vA)));
            xb_vec2Nx8 loS = IVP_PACKL2NX24(IVP_CVT24U2NX16(
                IVP_UNPKU2NX8_0(*(const xb_vec2Nx8 *)&vB),
                IVP_UNPKU2NX8_0(*(const xb_vec2Nx8 *)&vA)));
            xb_vec2Nx8U hi8 = *(const xb_vec2Nx8U *)(const void *)&hiS;
            xb_vec2Nx8U lo8 = *(const xb_vec2Nx8U *)(const void *)&loS;
            if (k == 0) {
                accH = IVP_MULUS2N8XR16(hi8, G[0]);
                accL = IVP_MULUS2N8XR16(lo8, G[0]);
            } else {
                IVP_MULUSPA2N8XR16(accH, hi8, hi8, G[k]);
                IVP_MULUSPA2N8XR16(accL, lo8, lo8, G[k]);
            }
        }
        xb_vec2Nx8 L1s = IVP_PACKVRNR2NX24(accL, 8);       /* L >> 8, truncate */
        xb_vec2Nx8U L1 = *(const xb_vec2Nx8U *)(const void *)&L1s;
        IVP_MULUSPA2N8XR16(accH, L1, L1, 1);               /* accH += L>>8    */
        xb_vec2Nx8U out8 = IVP_PACKVRU2NX24(accH, 8);      /* (H+L1+128)>>8   */
        for (int i = 0; i < 512; ++i) g_out[i] = 0xEE;
        st2N(out8, g_out);

        int ok = 1;
        for (int lane = 0; lane < 64; ++lane) {
            uint32_t s = 0;
            for (int k = 0; k < 7; ++k) s += (uint32_t)G[k] * (uint32_t)t7[k][lane];
            uint32_t want = (s + 32768u) >> 16;
            if (want > 255u) want = 255u;
            if (g_out[lane] != (uint8_t)want) {
                ok = 0;
                printf("   T19 lane %d: got %d want %u (s=%u)\n", lane, g_out[lane], want, s);
            }
        }
        chk(ok, "T19 blur vertical split-plane MAC == (sum g*tmp + 32768)>>16");
    }

    /* ---------- T20: 2Nx24 lane sign domain (why T19 splits the MAC) ------
     * PACKVRU2NX24 / PACKVRNR2NX24 read the 24-bit accumulator lanes as
     * SIGNED (cstub de-interleave sign-extends bit 23), and PACKVRU clamps a
     * negative lane to 0. A lane >= 0x800000 therefore comes back as 0 --
     * the blur vertical sum (up to 256*65280 = 0xFF0000) cannot live in one
     * accumulator. Lanes are built by MAC arithmetic (no raw load): even
     * lanes 255*32767 + 255*129 + 127 = 0x7FFFFF, odd lanes ... + 1 = 0x800000.
     * Pin: (a) PACKVRU(v,16): 0x7FFFFF -> 128, 0x800000 -> 0 (adjacent
     * inputs, output drops to 0 = bit-23 sign extension);
     * (b) PACKVRNR(v,8) is a truncating v>>8, PACKVRU(v,8) is (v+128)>>8. */
    {
        ALIGN64 uint8_t b255[64], b127[64], b241[64];
        memset(b255, 255, 64); memset(b127, 127, 64); memset(b241, 241, 64);
        xb_vec2Nx8U v255 = ld2N(b255), v127 = ld2N(b127), v241 = ld2N(b241);

        xb_vec2Nx24 acc = IVP_MULUS2N8XR16(v255, 32767);   /* 8,355,585 */
        IVP_MULUSPA2N8XR16(acc, v255, v255, 129);          /* +32,895 */
        ALIGN64 uint8_t b_fin[64];                          /* even: +127, odd: +128 */
        for (int i = 0; i < 64; ++i) b_fin[i] = (uint8_t)((i & 1) ? 128 : 127);
        IVP_MULUSPA2N8XR16(acc, ld2N(b_fin), ld2N(b_fin), 1);

        xb_vec2Nx8U u16sh = IVP_PACKVRU2NX24(acc, 16);
        for (int i = 0; i < 512; ++i) g_out[i] = 0xEE;
        st2N(u16sh, g_out);
        int ok = (g_out[0] == 128) && (g_out[1] == 0) && (g_out[2] == 128) && (g_out[3] == 0);
        if (!ok) printf("   T20 PACKVRU16: %d %d %d %d (want 128 0 128 0)\n",
                        g_out[0], g_out[1], g_out[2], g_out[3]);
        chk(ok, "T20 PACKVRU2NX24: lanes >= 0x800000 read as negative -> 0");

        xb_vec2Nx24 acc2 = IVP_MULUS2N8XR16(v255, 52);     /* 13,260 */
        IVP_MULUSPA2N8XR16(acc2, v241, v241, 1);           /* +241 = 0x34AB */
        xb_vec2Nx8 nr8 = IVP_PACKVRNR2NX24(acc2, 8);
        for (int i = 0; i < 512; ++i) g_out[i] = 0xEE;
        st2N(*(const xb_vec2Nx8U *)(const void *)&nr8, g_out);
        ok = (g_out[0] == 0x34);
        xb_vec2Nx8U vr8 = IVP_PACKVRU2NX24(acc2, 8);
        st2N(vr8, g_out);
        ok = ok && (g_out[1] == 0x35);              /* (0x34AB+0x80)>>8 = 0x35 */
        if (!ok) printf("   T20 shift8: %d %d (want %d %d)\n", g_out[0], g_out[1], 0x34, 0x35);
        chk(ok, "T20 PACKVRNR(v,8)=v>>8 trunc; PACKVRU(v,8)=(v+128)>>8");
    }

    /* ---------- T21: BRIEF gather chain ------------------------------------
     * IVP_GATHERANX16U(base, offs) sets a gsr whose lane[i] is the 16-bit load
     * at BYTE address base+offs[i] (u16 offsets, u16 elements); the low byte of
     * each lane is the pixel at base+offs[i]. IVP_GATHERD2NX8U(gb, gc) then
     * produces 64 bytes = [ low bytes of gc's 32 lanes | low bytes of gb's 32
     * lanes ] - the SECOND gsr argument lands in lanes 0-31. Two gathers per
     * operand plus one D2NX8U therefore rebuilds the 64 pixels at offs[0..63]
     * in lane order. */
    {
        ALIGN64 uint8_t img[1024];
        for (int i = 0; i < 1024; ++i) img[i] = (uint8_t)((i * 37 + 11) & 0xFF);
        ALIGN64 uint16_t offs[64];
        unsigned rs = 0x2545u;
        for (int i = 0; i < 64; ++i) {
            rs = rs * 1103515245u + 12345u;
            offs[i] = (uint16_t)((rs >> 16) & 0x3FF);   /* jumbled, < 1024 */
        }
        xb_vecNx16U oA = *(const xb_vecNx16U *)(const void *)(offs);
        xb_vecNx16U oB = *(const xb_vecNx16U *)(const void *)(offs + 32);
        xb_gsr ga = IVP_GATHERANX16U((const unsigned short *)(const void *)img, oA);
        xb_gsr gb = IVP_GATHERANX16U((const unsigned short *)(const void *)img, oB);
        xb_vec2Nx8U v = IVP_GATHERD2NX8U(gb, ga);
        for (int i = 0; i < 512; ++i) g_out[i] = 0xEE;
        st2N(v, g_out);
        int ok = 1;
        for (int lane = 0; lane < 64; ++lane)
            if (g_out[lane] != img[offs[lane]]) {
                ok = 0;
                printf("   T21 lane %d: got %d want %d (off=%u)\n",
                       lane, g_out[lane], img[offs[lane]], offs[lane]);
            }
        chk(ok, "T21 GATHERANX16U+GATHERD2NX8U = 64 px at u16 offs, [A|B] order");
    }

    /* ---------- T22: PACKVRNR2NX24 overflow after shift: wrap or saturate --
     * The resize horizontal needs Lm = (s>>4)&255 from s up to 522,240
     * (s>>4 up to 32,640 > 255): the pack must WRAP (keep the low 8 bits of
     * the shifted lane), not saturate, for the Hm/Lm byte-plane decomposition
     * to be exact. Lanes: s = 62*p + 195 (p = 3..255 -> s>>4 = 23..1000). */
    {
        ALIGN64 uint8_t bp[64], b195[64];
        for (int i = 0; i < 64; ++i) bp[i] = (uint8_t)(i * 4 + 3);
        memset(b195, 195, 64);
        xb_vec2Nx24 acc = IVP_MULUS2N8XR16(ld2N(bp), 62);
        IVP_MULUSPA2N8XR16(acc, ld2N(b195), ld2N(b195), 1);
        xb_vec2Nx8 lm = IVP_PACKVRNR2NX24(acc, 4);
        for (int i = 0; i < 512; ++i) g_out[i] = 0xEE;
        st2N(*(const xb_vec2Nx8U *)(const void *)&lm, g_out);
        int ok = 1;
        for (int lane = 0; lane < 64; ++lane) {
            uint32_t s = 62u * bp[lane] + 195u;
            uint8_t want = (uint8_t)((s >> 4) & 0xFF);
            if (g_out[lane] != want) {
                ok = 0;
                printf("   T22 lane %d: got %d want %d (s=%u, s>>4=%u)\n",
                       lane, g_out[lane], want, s, s >> 4);
            }
        }
        chk(ok, "T22 PACKVRNR2NX24(v,4) wraps: (v>>4)&255, no saturate");
    }

    /* ---------- T23: PACKVRNR2NX24_0/_1 -> u16 lane halves ----------------
     * Resize vertical reads each tap's 24-bit accumulator as (value>>8) into
     * u16 lanes. Two candidate lane mappings are discriminated: "halves"
     * (_0 -> lanes 0..31, _1 -> 32..63) vs "even/odd" (_0 -> even lanes, _1
     * odd - the aEven/aOdd naming in xsTileLib downScale.c). Lanes:
     * s = 16*(i+1)*128 + 7 -> s>>4 = 128*(i+1) (128..8192), strictly
     * increasing so lane order is pinned too. (T24c pins the same mapping
     * from an independently known vector.) */
    {
        ALIGN64 uint8_t bp[64], b7[64];
        for (int i = 0; i < 64; ++i) bp[i] = (uint8_t)(i + 1);
        memset(b7, 7, 64);
        xb_vec2Nx24 acc = IVP_MULUS2N8XR16(ld2N(bp), 2048);
        IVP_MULUSPA2N8XR16(acc, ld2N(b7), ld2N(b7), 1);
        xb_vecNx16U h0 = IVP_PACKVRNR2NX24_0(acc, 4);
        xb_vecNx16U h1 = IVP_PACKVRNR2NX24_1(acc, 4);
        ALIGN64 uint16_t w0[32], w1[32];
        xb_vecNx16U *q0 = (xb_vecNx16U *)(void *)w0;
        xb_vecNx16U *q1 = (xb_vecNx16U *)(void *)w1;
        IVP_SVNX16U_IP(h0, q0, (int)sizeof(h0));
        IVP_SVNX16U_IP(h1, q1, (int)sizeof(h1));
        int map = -1;   /* 0 = halves, 1 = even/odd */
        for (int lane = 0; lane < 64; ++lane) {
            uint16_t halves = (lane < 32) ? w0[lane] : w1[lane - 32];
            uint16_t eo     = (lane & 1) ? w1[lane >> 1] : w0[lane >> 1];
            if (halves != 128 * (lane + 1) && eo == 128 * (lane + 1)) map = 1;
            if (eo != 128 * (lane + 1) && halves == 128 * (lane + 1)) map = 0;
        }
        int ok = map >= 0;
        for (int lane = 0; lane < 64 && ok; ++lane) {
            uint16_t got = (map == 0) ? ((lane < 32) ? w0[lane] : w1[lane - 32])
                                      : ((lane & 1) ? w1[lane >> 1] : w0[lane >> 1]);
            if (got != 128 * (lane + 1)) {
                ok = 0;
                printf("   T23 lane %d: got %u want %u\n", lane, got, 128 * (lane + 1));
            }
        }
        chk(ok, map == 0 ? "T23 PACKVRNR2NX24_0/1: trunc >>4, lane halves [0..31|32..63], u16"
                         : "T23 PACKVRNR2NX24_0/1: trunc >>4, lane interleave [even|odd], u16");
    }

    /* ---------- T24: MULUSI2NX8X16 / MULUSAI2NX8X16 per-lane weights -------
     * Resize horizontal tap sum: s[lane] = px0[lane]*w0[lane] +
     * px1[lane]*w1[lane] with SIGNED 16-bit per-lane weights. Readout is via
     * PACKVRNR2NX24(v,4) -> 2Nx8 bytes (T22: lane order identity, wraps
     * mod 256) so the check does NOT depend on the still-suspect _0/_1 u16
     * halves. Stage (a)/(b) pin the c/d lane mapping with constant weights
     * 16 vs 8 and odd pixels p = 2i+1: byte out is p for weight 16 and
     * p>>1 for weight 8 - distinguishes "halves" (c -> lanes 0..31, d ->
     * 32..63) from "even/odd" (c -> even lanes, d -> odd lanes; the aEven/
     * aOdd naming in xsTileLib downScale.c hints at this). Stage (c) then
     * stores the SAME s through PACKVRNR2NX24_0/_1 u16 halves and compares
     * against the now-known per-lane values to pin _0/_1's mapping too
     * (T23 follow-up). Stage (d) mixed per-lane weights end to end. */
    {
        ALIGN64 uint8_t p0[64], p1[64];
        for (int i = 0; i < 64; ++i) {
            p0[i] = (uint8_t)((i * 2 + 1) & 0xFF);
            p1[i] = (uint8_t)((i * 2 + 33) & 0xFF);
        }
        ALIGN64 int16_t wc[32], wd[32];
        for (int j = 0; j < 32; ++j) { wc[j] = 16; wd[j] = 8; }
        xb_vecNx16 vc = *(const xb_vecNx16 *)(const void *)wc;
        xb_vecNx16 vd = *(const xb_vecNx16 *)(const void *)wd;
        xb_vec2Nx8U v0u = ld2N(p0), v1u = ld2N(p1);
        const xb_vec2Nx8 v0s = *(const xb_vec2Nx8 *)(const void *)&v0u;
        const xb_vec2Nx8 v1s = *(const xb_vec2Nx8 *)(const void *)&v1u;

        ALIGN64 uint8_t rb[64];
        ALIGN64 uint16_t r0[32], r1[32];
        xb_vec2Nx8U *rp = (xb_vec2Nx8U *)(void *)rb;

        /* (a) MULUSI alone: s = p0 * (c=16 | d=8). Three candidate lane
         * mappings for the c/d pair: h0 halves (c -> 0..31), h1 c -> even
         * lanes, h2 c -> odd lanes (the aEven/aOdd naming in xsTileLib
         * downScale.c hints at an interleave). First T24a run showed even
         * lanes took d and odd lanes took c -> h2. */
        xb_vec2Nx24 sa = IVP_MULUSI2NX8X16(v0s, vc, vd);
        IVP_SV2NX8U_IP(IVP_PACKVRNR2NX24(sa, 4), rp, 64);
        /* weight of `lane' under hypothesis h for c=16, d=8 */
        struct Wof { static int w(int h, int lane) {
            if (h == 0) return (lane < 32) ? 16 : 8;
            if (h == 1) return (lane & 1) ? 8 : 16;
            return (lane & 1) ? 16 : 8;
        } };
        auto pin = [&](const uint8_t *pp, const char *tag, int *mapOut) -> int {
            int found = -1;
            for (int h = 0; h < 3; ++h) {
                int all = 1;
                for (int lane = 0; lane < 64; ++lane)
                    if (rb[lane] != (uint8_t)((uint32_t)pp[lane] * Wof::w(h, lane) >> 4)) { all = 0; break; }
                if (all) { if (found >= 0) { printf("   %s: ambiguous\n", tag); return 0; } found = h; }
            }
            if (found < 0) {
                printf("   %s: no candidate mapping fits; first lanes:\n", tag);
                for (int lane = 0; lane < 64; ++lane) {
                    int want0 = (uint32_t)pp[lane] * Wof::w(0, lane) >> 4;
                    int want1 = (uint32_t)pp[lane] * Wof::w(1, lane) >> 4;
                    int want2 = (uint32_t)pp[lane] * Wof::w(2, lane) >> 4;
                    if (rb[lane] != want0 && rb[lane] != want1 && rb[lane] != want2)
                        printf("     lane %d: got %u want h0 %d h1 %d h2 %d\n",
                               lane, rb[lane], want0, want1, want2);
                }
                return 0;
            }
            *mapOut = found;
            return 1;
        };
        int mapA = -1, mapB = -1;
        int okA = pin(p0, "T24a", &mapA);
        chk(okA, mapA == 0 ? "T24a MULUSI: c->lanes 0..31, d->32..63"
                          : mapA == 1 ? "T24a MULUSI: c->even lanes, d->odd lanes"
                                      : "T24a MULUSI: c->odd lanes, d->even lanes");

        /* (b) MULUSAI accumulate: s = 0*p0 + p1 * (c=16 | d=8) */
        ALIGN64 int16_t wz[32];
        for (int j = 0; j < 32; ++j) wz[j] = 0;
        xb_vecNx16 vz = *(const xb_vecNx16 *)(const void *)wz;
        xb_vec2Nx24 sb = IVP_MULUSI2NX8X16(v0s, vz, vz);
        IVP_MULUSAI2NX8X16(sb, v1s, vc, vd);
        rp = (xb_vec2Nx8U *)(void *)rb;    /* _IP increments rp - reset */
        IVP_SV2NX8U_IP(IVP_PACKVRNR2NX24(sb, 4), rp, 64);
        int okB = pin(p1, "T24b", &mapB);
        if (okB && mapA != mapB) { okB = 0;
            printf("   T24b: MULUSAI map (%d) != MULUSI map (%d)\n", mapB, mapA); }
        chk(okB, "T24b MULUSAI2NX8X16 same lane map, accumulates");

        /* (c) cross-check _0/_1 (T23 pinned even/odd) against known sa */
        xb_vecNx16U *q0 = (xb_vecNx16U *)(void *)r0;
        xb_vecNx16U *q1 = (xb_vecNx16U *)(void *)r1;
        IVP_SVNX16U_IP(IVP_PACKVRNR2NX24_0(sa, 4), q0, 64);
        IVP_SVNX16U_IP(IVP_PACKVRNR2NX24_1(sa, 4), q1, 64);
        int okC = mapA >= 0;
        if (okC) for (int lane = 0; lane < 64; ++lane) {
            uint32_t want = (uint32_t)p0[lane] * (uint32_t)Wof::w(mapA, lane) >> 4;
            uint16_t got = (lane & 1) ? r1[lane >> 1] : r0[lane >> 1];
            if (got != (uint16_t)want) { okC = 0;
                printf("   T24c lane %d: got %u want %u\n", lane, got, want); }
        }
        chk(okC, "T24c _0/_1 even/odd readout agrees with byte readout");

        /* (d) mixed per-lane weights through the full tap-sum form; the
         * vecNx16 operands are laid out per the map pinned in (a) */
        ALIGN64 int16_t w0[64], w1[64];
        ALIGN64 int16_t cw[32], dw[32], cx[32], dx[32];
        for (int i = 0; i < 64; ++i) {
            w0[i] = (int16_t)(900 + (i * 17) % 1100);
            w1[i] = (int16_t)(2048 - w0[i]);
        }
        for (int j = 0; j < 32; ++j) {
            if (mapA == 0) {           /* halves: c -> lanes 0..31, d -> 32..63 */
                cw[j] = w0[j];      dw[j] = w0[32 + j];
                cx[j] = w1[j];      dx[j] = w1[32 + j];
            } else if (mapA == 1) {    /* c -> even lanes, d -> odd lanes */
                cw[j] = w0[2 * j];  dw[j] = w0[2 * j + 1];
                cx[j] = w1[2 * j];  dx[j] = w1[2 * j + 1];
            } else {                   /* c -> odd lanes, d -> even lanes */
                cw[j] = w0[2 * j + 1]; dw[j] = w0[2 * j];
                cx[j] = w1[2 * j + 1]; dx[j] = w1[2 * j];
            }
        }
        xb_vec2Nx24 s = IVP_MULUSI2NX8X16(v0s,
            *(const xb_vecNx16 *)(const void *)cw,
            *(const xb_vecNx16 *)(const void *)dw);
        IVP_MULUSAI2NX8X16(s, v1s,
            *(const xb_vecNx16 *)(const void *)cx,
            *(const xb_vecNx16 *)(const void *)dx);
        rp = (xb_vec2Nx8U *)(void *)rb;    /* _IP increments rp - reset */
        IVP_SV2NX8U_IP(IVP_PACKVRNR2NX24(s, 4), rp, 64);
        int ok = 1;
        for (int lane = 0; lane < 64; ++lane) {
            uint32_t want = (uint32_t)p0[lane] * (uint32_t)w0[lane]
                          + (uint32_t)p1[lane] * (uint32_t)w1[lane];
            /* byte readout wraps mod 256 (T22) - compare (want>>4) mod 256 */
            if (rb[lane] != (uint8_t)(want >> 4)) {
                ok = 0;
                printf("   T24d lane %d: got %u want %u\n", lane, rb[lane],
                       (want >> 4) & 0xFF);
            }
        }
        chk(ok, "T24d MULUSI/AI tap sum s = p0*w0 + p1*w1 per lane");
    }

    /* ---------- T25: CVT24U2NX16(a, b) lane order --------------------------
     * Resize vertical combines the summed u16 halves back into 64x24 lanes
     * before the final PACKVRU(v,2) = (v+2)>>2: a must supply lanes 32..63,
     * b lanes 0..31 (the order orb_blur_vp6.c uses). d[j] -> lane j with
     * value 16j+3 (>>4 = j); c[j] -> lane 32+j with value 16(j+20)+9
     * (>>4 = j+20, so a swap is visible). */
    {
        ALIGN64 uint16_t cd[32], dd[32];
        for (int j = 0; j < 32; ++j) {
            cd[j] = (uint16_t)(16 * (j + 20) + 9);
            dd[j] = (uint16_t)(16 * j + 3);
        }
        xb_vecNx16U a = *(const xb_vecNx16U *)(const void *)cd;
        xb_vecNx16U b = *(const xb_vecNx16U *)(const void *)dd;
        xb_vec2Nx24 cv = IVP_CVT24U2NX16(a, b);
        xb_vec2Nx8 rd = IVP_PACKVRNR2NX24(cv, 4);
        for (int i = 0; i < 512; ++i) g_out[i] = 0xEE;
        st2N(*(const xb_vec2Nx8U *)(const void *)&rd, g_out);
        int ok = 1;
        for (int lane = 0; lane < 64; ++lane) {
            int want = (lane < 32) ? lane : (lane - 32 + 20);
            if (g_out[lane] != (uint8_t)want) {
                ok = 0;
                printf("   T25 lane %d: got %d want %d\n", lane, g_out[lane], want);
            }
        }
        chk(ok, "T25 CVT24U2NX16(a,b): a->lanes 32..63, b->lanes 0..31");
    }

    /* ---------- T26: ADDNX16U elementwise u16 add -------------------------- */
    {
        ALIGN64 uint16_t ua[32], ub[32];
        for (int j = 0; j < 32; ++j) {
            ua[j] = (uint16_t)(1000 + j);
            ub[j] = (uint16_t)(2000 + 2 * j);
        }
        xb_vecNx16U a = *(const xb_vecNx16U *)(const void *)ua;
        xb_vecNx16U b = *(const xb_vecNx16U *)(const void *)ub;
        xb_vecNx16U csum = IVP_ADDNX16U(a, b);
        ALIGN64 uint16_t uc[32];
        xb_vecNx16U *qc = (xb_vecNx16U *)(void *)uc;
        IVP_SVNX16U_IP(csum, qc, (int)sizeof(csum));
        int ok = 1;
        for (int j = 0; j < 32; ++j)
            if (uc[j] != (uint16_t)(3000 + 3 * j)) {
                ok = 0;
                printf("   T26 j=%d: got %u want %u\n", uc[j], j, 3000 + 3 * j);
            }
        chk(ok, "T26 ADDNX16U: elementwise u16 add");
    }

    /* ---------- T27: SB2N_IP packed bool store -----------------------------
     * brief_vp6 packs the 64 LTU compare lanes straight into 8 descriptor
     * bytes. Pin the lane->bit order and the pointer contract: cstub
     * Function_ shows an 8B-aligned pointer (b&7 trap otherwise) and one
     * 64-bit little-endian write, i.e. byte j bit b = lane j*8+b, and the
     * immediate post-increment is in bytes. */
    {
        ALIGN64 uint8_t bt0[64], bt1[64];
        for (int i = 0; i < 64; ++i) {
            bt0[i] = (uint8_t)(i & 63);       /* lane i value i&63 */
            bt1[i] = 32;                      /* true iff (i&63) < 32 */
        }
        xb_vec2Nx8U t0 = ld2N(bt0), t1 = ld2N(bt1);
        ALIGN64 uint8_t packed[16];
        memset(packed, 0xEE, sizeof packed);
        vbool2N *pp = (vbool2N *)(void *)packed;
        IVP_SB2N_IP(IVP_LTU2NX8(t0, t1), pp, 8);
        int ok = 1;
        for (int j = 0; j < 8; ++j) {
            uint8_t want = (j < 4) ? 0xFF : 0x00;   /* lanes 0-31 true */
            if (packed[j] != want) {
                ok = 0;
                printf("   T27 byte %d: got %02X want %02X\n", j, packed[j], want);
            }
        }
        /* post-increment advanced the pointer by the immediate (bytes) */
        if ((uint8_t *)pp != packed + 8) {
            ok = 0;
            printf("   T27 post-inc: pp = %+d (want +8)\n", (int)((uint8_t *)pp - packed));
        }
        chk(ok, "T27 SB2N_IP: lane i -> bit i LE, 8B store, byte post-increment");
    }

    /* ---------- T28: u16 halfword gather chain (brief offsets stage) --------
     * The vectorised offset stage gathers r/c values from u16 planes with
     * GATHERANX16U (16b offset / 16b element, gscontrol 0x41): lane[i] is the
     * u16 at BYTE address base+off[i]. The real Gather/Scatter unit NOOPs an
     * element address unaligned to ElemSize (=2), so the kernel stores its
     * index tables pre-multiplied by 2 (always even); the cstub does not model
     * the alignment check, so this probe pins only the element/lane semantics
     * with even offsets. */
    {
        ALIGN64 uint16_t plane[64];
        for (int i = 0; i < 64; ++i) plane[i] = (uint16_t)(1000 + i * 7);
        ALIGN64 uint16_t offs[32];
        unsigned rs = 0x8765u;
        for (int i = 0; i < 32; ++i) {
            rs = rs * 1103515245u + 12345u;
            offs[i] = (uint16_t)(2 * ((rs >> 16) & 63));   /* jumbled, even */
        }
        xb_vecNx16U ov = *(const xb_vecNx16U *)(const void *)offs;
        xb_gsr g = IVP_GATHERANX16U((const unsigned short *)(const void *)plane, ov);
        xb_vecNx16U got = IVP_GATHERDNX16U(g);
        ALIGN64 uint16_t G[32];
        memset(G, 0xEE, sizeof G);
        xb_vecNx16U *pg = (xb_vecNx16U *)(void *)G;
        IVP_SVNX16U_IP(got, pg, (int)sizeof(got));
        int ok = 1;
        for (int i = 0; i < 32; ++i)
            if (G[i] != plane[offs[i] / 2]) {
                ok = 0;
                printf("   T28 lane %d: got %u want %u (off=%u)\n",
                       i, G[i], plane[offs[i] / 2], offs[i]);
            }
        chk(ok, "T28 GATHERANX16U+GATHERDNX16U: u16 elems, lane i = *(u16*)(base+off[i])");
    }

    /* ---------- T29: biased clamp + linearise chain (brief offsets stage) ---
     * Per lane with r' = r+64 stored in a u16 plane (r in [-32,32]):
     *   yb  = MIN(MAX(r' + (cy-64+BIAS), BIAS), BIAS+h-1)   == clamp(cy+r,0,h-1)+BIAS
     *   yo  = yb - (BIAS+oy)                                == clamped y - origin_y
     *   xo  = MIN(MAX(c' + (cx-64+BIAS), BIAS), BIAS+w-1) - BIAS
     *   off = low16(yo*pitch) + xo                          == (y-oy)*pitch + x
     * Includes one lane with
     * clamped y < oy (cy=12, r=-32 -> y=0 < oy=7) to pin that wrap.
     *
     * SPLAT SEMANTICS (learned here, first version returned all-zero yo/xo):
     * IVP_MOVNX16U_FROM16U(t) loads ONE u16 from &t into LANE 0 ONLY (lanes
     * 1..31 = 0) - it is a lane-0 memory load, not a broadcast. Broadcasting
     * is a separate op: IVP_REPNX16U(v, 0) copies lane 0 to all 32 lanes. */
    {
        const int cy = 12, cx = 300, w = 320, h = 240, oy = 7, pitch = 320;
        enum { BF_BIAS = 1024 };
        ALIGN64 uint16_t rp[32], cp[32];
        for (int j = 0; j < 32; ++j) {
            rp[j] = (uint16_t)(-32 + (int)((unsigned)(j * 3) % 57) + 64);  /* r in [-32,24] */
            cp[j] = (uint16_t)(-32 + (int)((unsigned)(j * 5) % 57) + 64);  /* c in [-32,24] */
        }
        rp[0] = (uint16_t)(-32 + 64);          /* lane 0: y = -20 -> clamp 0 < oy */
        xb_vecNx16U vr = *(const xb_vecNx16U *)(const void *)rp;
        xb_vecNx16U vc = *(const xb_vecNx16U *)(const void *)cp;
        const xb_vecNx16U vKy = IVP_REPNX16U(IVP_MOVNX16U_FROM16U((xb_int16U)(cy - 64 + BF_BIAS)), 0);
        const xb_vecNx16U vKx = IVP_REPNX16U(IVP_MOVNX16U_FROM16U((xb_int16U)(cx - 64 + BF_BIAS)), 0);
        const xb_vecNx16U vB0 = IVP_REPNX16U(IVP_MOVNX16U_FROM16U((xb_int16U)BF_BIAS), 0);
        const xb_vecNx16U vBy1 = IVP_REPNX16U(IVP_MOVNX16U_FROM16U((xb_int16U)(BF_BIAS + h - 1)), 0);
        const xb_vecNx16U vBx1 = IVP_REPNX16U(IVP_MOVNX16U_FROM16U((xb_int16U)(BF_BIAS + w - 1)), 0);
        const xb_vecNx16U vBo = IVP_REPNX16U(IVP_MOVNX16U_FROM16U((xb_int16U)(BF_BIAS + oy)), 0);
        const xb_vecNx16U vKp = IVP_REPNX16U(IVP_MOVNX16U_FROM16U((xb_int16U)pitch), 0);
        xb_vecNx16U yb = IVP_MINUNX16U(IVP_MAXUNX16U(IVP_ADDNX16U(vr, vKy), vB0), vBy1);
        xb_vecNx16U xb = IVP_MINUNX16U(IVP_MAXUNX16U(IVP_ADDNX16U(vc, vKx), vB0), vBx1);
        xb_vecNx16U yo = IVP_SUBNX16U(yb, vBo);
        xb_vecNx16U xo = IVP_SUBNX16U(xb, vB0);
        xb_vecNx16U off = IVP_ADDNX16U(
            *(const xb_vecNx16U *)(const void *)&IVP_PACKLNX48(IVP_MULUUNX16U(yo, vKp)), xo);
        ALIGN64 uint16_t O[32], Ydump[32], Xdump[32];
        memset(O, 0xEE, sizeof O);
        {
            xb_vecNx16U *qy = (xb_vecNx16U *)(void *)Ydump;
            xb_vecNx16U *qx = (xb_vecNx16U *)(void *)Xdump;
            IVP_SVNX16U_IP(yo, qy, (int)sizeof(yo));
            IVP_SVNX16U_IP(xo, qx, (int)sizeof(xo));
        }
        xb_vecNx16U *qo = (xb_vecNx16U *)(void *)O;
        IVP_SVNX16U_IP(off, qo, (int)sizeof(off));
        int ok = 1;
        for (int j = 0; j < 32; ++j) {
            int r = (int)rp[j] - 64, c = (int)cp[j] - 64;
            int y = cy + r, x = cx + c;
            if (y < 0) y = 0; else if (y >= h) y = h - 1;
            if (x < 0) x = 0; else if (x >= w) x = w - 1;
            uint16_t want = (uint16_t)((y - oy) * pitch + x);
            if (O[j] != want) {
                ok = 0;
                if (j < 6)
                    printf("   T29 lane %d: got %u want %u (r=%d c=%d y=%d x=%d yo=%u xo=%u)\n",
                           j, O[j], want, r, c, y, x, Ydump[j], Xdump[j]);
            }
        }
        chk(ok, "T29 MINU/MAXU/SUB/MULUU/PACKL chain == scalar clamp+offset (mod 2^16)");
    }

    /* ---------- T30: MULANX16UPACKL == ADD(PACKL(MULUU)) fused form --------
     * Lane semantics from cstub: a += low16(b*c), all mod 2^16 (b, c read as
     * signed shorts, product's low 16 bits - identical to the unsigned low
     * half). One op replaces MULUU (Nx48 result!) + PACKL + ADD, so loop 2 of
     * brief_kernel_ivp can stay entirely in Nx16 registers (xsTileLib's
     * ORBAngleBRIEF.c uses the same fused op for y*pitch+x). */
    {
        const int oy = 7, pitch = 320;
        enum { BF_BIAS = 1024 };
        ALIGN64 uint16_t yb[32], xq[32];
        for (int j = 0; j < 32; ++j) {
            yb[j] = (uint16_t)(BF_BIAS + ((j * 11) % 62) - 30);  /* y in [-30,31] */
            xq[j] = (uint16_t)((j * 37) % 640);                  /* x in [0,639]  */
        }
        yb[0] = (uint16_t)BF_BIAS;                 /* y = 0 < oy: negative yo wrap */
        yb[1] = (uint16_t)(BF_BIAS + 61);          /* large y*pitch product        */
        xb_vecNx16U vyb = *(const xb_vecNx16U *)(const void *)yb;
        xb_vecNx16U vx = *(const xb_vecNx16U *)(const void *)xq;
        const xb_vecNx16U vBo = IVP_REPNX16U(IVP_MOVNX16U_FROM16U((xb_int16U)(BF_BIAS + oy)), 0);
        const xb_vecNx16U vKp = IVP_REPNX16U(IVP_MOVNX16U_FROM16U((xb_int16U)pitch), 0);
        xb_vecNx16U yo = IVP_SUBNX16U(vyb, vBo);
        /* fused: off = xo + low16(yo*pitch), xo == x (origin_x = 0 form) */
        xb_vecNx16U off = vx;
        IVP_MULANX16UPACKL(off, yo, vKp);
        /* reference: the T29 three-op chain */
        xb_vecNx16U ref = IVP_ADDNX16U(
            IVP_PACKLNX48(IVP_MULUUNX16U(yo, vKp)),
            vx);
        ALIGN64 uint16_t O[32], R[32];
        xb_vecNx16U *po = (xb_vecNx16U *)(void *)O, *pr = (xb_vecNx16U *)(void *)R;
        IVP_SVNX16U_IP(off, po, (int)sizeof(off));
        IVP_SVNX16U_IP(ref, pr, (int)sizeof(ref));
        int ok = 1;
        for (int j = 0; j < 32; ++j) {
            int y = (int)yb[j] - BF_BIAS, x = (int)xq[j];
            uint16_t want = (uint16_t)((y - oy) * pitch + x);
            if (O[j] != want || R[j] != want) {
                ok = 0;
                if (j < 6)
                    printf("   T30 lane %d: fused %u chain %u want %u (y=%d x=%d)\n",
                           j, O[j], R[j], want, y, x);
            }
        }
        chk(ok, "T30 MULANX16UPACKL(a,b,c) == a + low16(b*c) == 3-op chain (mod 2^16)");
    }

    /* ---------- T31: fixed-point rotation chain (brief_int_vp6.c) -----------
     * Target semantics (xsTileLib ORBAngleBRIEF.c form, Q7 sin/cos):
     *   wr = MULP2NX8(NEG2NX8(Y), sin, X, cos)   lane = X*cos - Y*sin (+?)
     *   r8 = PACKVR2NX24(wr, 7)                  lane = (X*cos - Y*sin (+?)) >> 7
     * plus the pieces around it: scalar-ctor broadcast, LSR2NX8_X LUT splat,
     * and the UNPK_L/H even/odd byte split that motivates the bit-permutation
     * in the int kernel's descriptor fold. */
#ifndef IVP_SELI_8B_INTERLEAVE_1_LO
#define IVP_SELI_8B_INTERLEAVE_1_LO 59
#endif
#ifndef IVP_SELI_8B_INTERLEAVE_1_HI
#define IVP_SELI_8B_INTERLEAVE_1_HI 60
#endif
    {
        /* ctor broadcast? */
        xb_vec2Nx8 v64 = 64;
        uint8_t B[64];
        *(xb_vec2Nx8 *)(void *)B = v64;
        int ok = 1;
        for (int j = 0; j < 64; ++j) if (B[j] != 64) { ok = 0; break; }
        chk(ok, "T31a xb_vec2Nx8 v = 64 broadcasts the scalar to all 64 lanes");

        /* LSR2NX8_X = splat of one LUT byte */
        ALIGN64 int8_t lut[16];
        for (int j = 0; j < 16; ++j) lut[j] = (int8_t)(-120 + j * 16);
        lut[7] = 99;
        xb_vec2Nx8 vs = IVP_LSR2NX8_X(lut, 7);
        *(xb_vec2Nx8 *)(void *)B = vs;
        ok = 1;
        for (int j = 0; j < 64; ++j) if (B[j] != 99) { ok = 0; break; }
        chk(ok, "T31b IVP_LSR2NX8_X(lut, i) splats lut[i] to all 64 lanes");

        /* rotation MAC + rounding: products with fractional parts .25/.5/.75 */
        ALIGN64 int8_t X[64], Y[64];
        const int s7 = 100, c7 = -37;         /* arbitrary Q7 pair */
        for (int j = 0; j < 64; ++j) { X[j] = (int8_t)(j - 32); Y[j] = (int8_t)((j * 7) % 23 - 11); }
        ALIGN64 int8_t lut2[2] = { (int8_t)c7, (int8_t)s7 };
        xb_vec2Nx8 vX = *(const xb_vec2Nx8 *)(const void *)X;
        xb_vec2Nx8 vY = *(const xb_vec2Nx8 *)(const void *)Y;
        xb_vec2Nx8 vS = IVP_LSR2NX8_X(lut2 + 1, 0);
        xb_vec2Nx8 vC = IVP_LSR2NX8_X(lut2, 0);
        xb_vec2Nx24 wr = IVP_MULP2NX8(IVP_NEG2NX8(vY), vS, vX, vC);
        xb_vec2Nx8 r8 = IVP_PACKVR2NX24(wr, 7);
        *(xb_vec2Nx8 *)(void *)B = r8;
        int m_trunc = 1, m_round = 1, m_prebias = 1;
        for (int j = 0; j < 64; ++j) {
            int prod = (int)X[j] * c7 - (int)Y[j] * s7;   /* X*cos - Y*sin */
            int t = (int)(int8_t)B[j];
            if (t != prod >> 7) m_trunc = 0;
            if (t != (prod + 64) >> 7) m_round = 0;
            if (t != (prod + 128) >> 7) m_prebias = 0;
        }
        int nm = m_trunc + m_round + m_prebias;
        printf("   T31c rounding: trunc=%d round(prod+64)=%d prebias(prod+128)=%d%s\n",
               m_trunc, m_round, m_prebias, nm == 1 ? " (unique)" : " (AMBIGUOUS)");
        chk(nm == 1, "T31c MULP2NX8(NEG(Y),S,X,C)+PACKVR2NX24(,7) == exactly one rounding form");

        /* UNPK even/odd split (zero-extended). NOTE: the xsTileLib-style
         * SEL2NX8I(.., 59/60) + MOVNX16_FROM2NX8U chain does NOT reproduce
         * this in cstub (identity/garbage lane map — see T31d dump history);
         * the header's own IVP_UNPKU2NX8_0/_1 macros are the verified form. */
        xb_vecNx16 lo = IVP_UNPKU2NX8_0(r8);
        xb_vecNx16 hi = IVP_UNPKU2NX8_1(r8);
        ALIGN64 uint16_t LO[32], HI[32];
        *(xb_vecNx16 *)(void *)LO = lo;
        *(xb_vecNx16 *)(void *)HI = hi;
        ok = 1;
        for (int j = 0; j < 32; ++j) {
            if (LO[j] != (uint8_t)B[2 * j] || HI[j] != (uint8_t)B[2 * j + 1]) { ok = 0; break; }
        }
        chk(ok, "T31d IVP_UNPKU2NX8_0/_1 -> lanes = even/odd bytes, zero-extended");
    }

    printf("\n%s (%d failures)\n", g_fails ? "PROBE FAILURES" : "ALL PASS", g_fails);
    return g_fails ? 1 : 0;
}
