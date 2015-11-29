#ifndef __SECRIL_SHIM_H__
#define __SECRIL_SHIM_H__

#define LOG_TAG "secril-shim"
#define RIL_SHLIB

#include <cutils/properties.h>
#include <sys/cdefs.h>
#include <telephony/ril.h>
#include <utils/Log.h>
#include <dlfcn.h>
#include <stdlib.h>
#include <string.h>

#define RIL_LIB_PATH "/vendor/lib/libsec-ril.so"

enum variant_type {
	VARIANT_INIT,
	VARIANT_MAGURO,
	VARIANT_TORO,
	VARIANT_TOROPLUS,
	VARIANT_UNKNOWN
};

extern const char * requestToString(int request);

#endif /* __SECRIL_SHIM_H__ */
