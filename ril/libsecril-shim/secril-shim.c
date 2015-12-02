#include "secril-shim.h"

/* A copy of the original RIL function table. */
static const RIL_RadioFunctions *origRilFunctions;

/* A copy of the ril environment passed to RIL_Init. */
static const struct RIL_Env *rilEnv;

/* The tuna variant we're running on. */
static int tunaVariant = VARIANT_INIT;


static void onRequestShim(int request, void *data, size_t datalen, RIL_Token t)
{
	switch (request) {
		/* Necessary; RILJ may fake this for us if we reply not supported, but we can just implement it. */
		case RIL_REQUEST_GET_RADIO_CAPABILITY:
			; /* lol C standard */
			int raf = RAF_UNKNOWN;
			if (tunaVariant == VARIANT_MAGURO) {
				raf = RAF_GSM | RAF_GPRS | RAF_EDGE | RAF_HSUPA | RAF_HSDPA | RAF_HSPA | RAF_HSPAP | RAF_UMTS;
			} else if (tunaVariant == VARIANT_TORO || tunaVariant == VARIANT_TOROPLUS) {
				raf = RAF_LTE | RAF_IS95A | RAF_IS95B | RAF_1xRTT | RAF_EVDO_0 | RAF_EVDO_A | RAF_EVDO_B | RAF_EHRPD;
			}
			if (__predict_true(raf != RAF_UNKNOWN)) {
				RIL_RadioCapability rc[1] =
				{
					{ /* rc[0] */
						RIL_RADIO_CAPABILITY_VERSION, /* version */
						0, /* session */
						RC_PHASE_CONFIGURED, /* phase */
						raf, /* rat */
						{ /* logicalModemUuid */
							0,
						},
						RC_STATUS_SUCCESS /* status */
					}
				};
				RLOGW("%s: got request %s: replied with our implementation!\n", __func__, requestToString(request));
				rilEnv->OnRequestComplete(t, RIL_E_SUCCESS, rc, sizeof(rc));
				return;
			}
			/* else fallthrough to RIL_E_REQUEST_NOT_SUPPORTED */

		/* The following requests were introduced post-4.3. */
		case RIL_REQUEST_SIM_TRANSMIT_APDU_BASIC:
		case RIL_REQUEST_SIM_OPEN_CHANNEL: /* !!! */
		case RIL_REQUEST_SIM_CLOSE_CHANNEL:
		case RIL_REQUEST_SIM_TRANSMIT_APDU_CHANNEL:
		case RIL_REQUEST_NV_READ_ITEM:
		case RIL_REQUEST_NV_WRITE_ITEM:
		case RIL_REQUEST_NV_WRITE_CDMA_PRL:
		case RIL_REQUEST_NV_RESET_CONFIG:
		case RIL_REQUEST_SET_UICC_SUBSCRIPTION:
		case RIL_REQUEST_ALLOW_DATA:
		case RIL_REQUEST_GET_HARDWARE_CONFIG:
		case RIL_REQUEST_SIM_AUTHENTICATION:
		case RIL_REQUEST_GET_DC_RT_INFO:
		case RIL_REQUEST_SET_DC_RT_INFO_RATE:
		case RIL_REQUEST_SET_DATA_PROFILE:
		case RIL_REQUEST_SHUTDOWN: /* TODO: Is there something we can do for RIL_REQUEST_SHUTDOWN ? */
		case RIL_REQUEST_SET_RADIO_CAPABILITY:
		case RIL_REQUEST_START_LCE:
		case RIL_REQUEST_STOP_LCE:
		case RIL_REQUEST_PULL_LCEDATA:
			RLOGW("%s: got request %s: replied with REQUEST_NOT_SUPPPORTED.\n", __func__, requestToString(request));
			rilEnv->OnRequestComplete(t, RIL_E_REQUEST_NOT_SUPPORTED, NULL, 0);
			return;
	}

	RLOGD("%s: got request %s: forwarded to RIL.\n", __func__, requestToString(request));
	origRilFunctions->onRequest(request, data, datalen, t);
}

const RIL_RadioFunctions* RIL_Init(const struct RIL_Env *env, int argc, char **argv)
{
	RIL_RadioFunctions const* (*origRilInit)(const struct RIL_Env *env, int argc, char **argv);
	static RIL_RadioFunctions shimmedFunctions;
	void *origRil;
	char propBuf[PROPERTY_VALUE_MAX];

	if (__predict_true(tunaVariant == VARIANT_INIT)) {
		property_get("ro.product.subdevice", propBuf, "unknown");
		if (!strcmp(propBuf, "maguro")) {
			tunaVariant = VARIANT_MAGURO;
		} else if (!strcmp(propBuf, "toro")) {
			tunaVariant = VARIANT_TORO;
		} else if (!strcmp(propBuf, "toroplus")) {
			tunaVariant = VARIANT_TOROPLUS;
		} else {
			tunaVariant = VARIANT_UNKNOWN;
		}
		RLOGD("%s: got tuna variant: %i", __func__, tunaVariant);
	}

	rilEnv = env;

	/* Open and Init the original RIL. */

	origRil = dlopen(RIL_LIB_PATH, RTLD_LOCAL);
	if (__predict_false(!origRil)) {
		RLOGE("%s: failed to load '" RIL_LIB_PATH  "': %s\n", __func__, dlerror());
		return NULL;
	}

	origRilInit = dlsym(origRil, "RIL_Init");
	if (__predict_false(!origRilInit)) {
		RLOGE("%s: couldn't find original RIL_Init!\n", __func__);
		dlclose(origRil);
		return NULL;
	}

	origRilFunctions = origRilInit(env, argc, argv);
	if (__predict_false(!origRilFunctions)) {
		RLOGE("%s: the original RIL_Init derped.\n", __func__);
		dlclose(origRil);
		return NULL;
	}

	/* Shim functions as needed. */
	shimmedFunctions = *origRilFunctions;
	shimmedFunctions.onRequest = onRequestShim;

	return &shimmedFunctions;
}
