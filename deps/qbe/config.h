/* QBE configuration for Fern compiler */

#if defined(__APPLE__)
    #if defined(__arm64__) || defined(__aarch64__)
        #define Deftgt T_arm64_apple
    #else
        #define Deftgt T_amd64_apple
    #endif
#elif defined(__linux__)
    #if defined(__aarch64__)
        #define Deftgt T_arm64
    #elif defined(__riscv)
        #define Deftgt T_rv64
    #else
        #define Deftgt T_amd64_sysv
    #endif
#else
    /* Default to amd64 sysv for other Unix-like systems */
    #define Deftgt T_amd64_sysv
#endif
