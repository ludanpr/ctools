/*
 * C Header file: alignment_crash_x86_64.h
 *
 */
#ifndef ALIGNMENT_CRASH_X86_64
#define ALIGNMENT_CRASH_X86_64 1

/**
 * @brief enable alignment check for i386 processors
 *
 * Intel's i386 processor family is quite tolerant in accepting misalignment
 * of data. This can lead to irritating bugs when ported to other architectures
 * that are not as tolerant.
 *
 * This function enables a check for this problem also for this family of processors,
 * such that you can be sure to detect this problem early.
 *
 * Based on: http://orchistro.tistory.com/206
 */

inline void enable_alignment_check_x86_64(void)
{
#if defined(__GNUC__)
#  if defined(__x86_64__)
    __asm__("pushf\n"
            "\torl $0x40000,(%%rsp)\n"
            "\tpopf"
            : : : "cc");
#  elif defined(__i386__)
    __asm__("pushf\n"
            "\torl %0x40000,(%%esp)\n"
            "\tpopf"
            : : : "cc");
#  endif
#elif defined(_MSC_VER)
#  if defined(_M_AMD64) || defined(_M_X64)
    __asm {
        pushf
        orl rsp,0x40000
        popf
    }
#  elif defined(_M_IX86)
    __asm {
        pushf
        orl esp,0x40000
        popf
    }
#  endif
#endif
}

#endif
