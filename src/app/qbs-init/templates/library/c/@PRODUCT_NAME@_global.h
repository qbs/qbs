#ifndef @PRODUCT_NAME_UPPER@_GLOBAL_H
#define @PRODUCT_NAME_UPPER@_GLOBAL_H

#if defined(_WIN32) || defined(WIN32)
#define @PRODUCT_NAME_UPPER@_DECL_EXPORT __declspec(dllexport)
#define @PRODUCT_NAME_UPPER@_DECL_IMPORT __declspec(dllimport)
#else
#define @PRODUCT_NAME_UPPER@_DECL_EXPORT __attribute__((visibility("default")))
#define @PRODUCT_NAME_UPPER@_DECL_IMPORT __attribute__((visibility("default")))
#endif

#if defined(@PRODUCT_NAME_UPPER@_STATIC_LIBRARY)
#define @PRODUCT_NAME_UPPER@_EXPORT
#else
#if defined(@PRODUCT_NAME_UPPER@_LIBRARY)
#define @PRODUCT_NAME_UPPER@_EXPORT @PRODUCT_NAME_UPPER@_DECL_EXPORT
#else
#define @PRODUCT_NAME_UPPER@_EXPORT @PRODUCT_NAME_UPPER@_DECL_IMPORT
#endif
#endif

#endif // @PRODUCT_NAME_UPPER@_GLOBAL_H
