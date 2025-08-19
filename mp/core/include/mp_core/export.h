#if __GNUC__
#define MP_EXPORT [[gnu::visibility("default")]]
#else
#define MP_EXPORT
#endif
