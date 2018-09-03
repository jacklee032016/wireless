#include "ah.h"
#include <errno.h>
#include "ar5xxx.h"

#define AR5K_DELAY(_n)		delay(_n)
#define AR5K_ELEMENTS(_array)	(sizeof(_array) / sizeof(_array[0]))




#define AR5K_RATES_11A { 8, {						\
	255, 255, 255, 255, 255, 255, 255, 255, 6, 4, 2, 0,		\
	7, 5, 3, 1, 255, 255, 255, 255, 255, 255, 255, 255,		\
	255, 255, 255, 255, 255, 255, 255, 255 }, {			\
	{ 1, IEEE80211_T_OFDM, 6000, 11, 0, 140, 0 },			\
	{ 1, IEEE80211_T_OFDM, 9000, 15, 0, 18, 0 },			\
	{ 1, IEEE80211_T_OFDM, 12000, 10, 0, 152, 2 },			\
	{ 1, IEEE80211_T_OFDM, 18000, 14, 0, 36, 2 },			\
	{ 1, IEEE80211_T_OFDM, 24000, 9, 0, 176, 4 },			\
	{ 1, IEEE80211_T_OFDM, 36000, 13, 0, 72, 4 },			\
	{ 1, IEEE80211_T_OFDM, 48000, 8, 0, 96, 4 },			\
	{ 1, IEEE80211_T_OFDM, 54000, 12, 0, 108, 4 } }			\
}

#define AR5K_RATES_11B { 4, {						\
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,	\
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,	\
	3, 2, 1, 0, 255, 255, 255, 255 }, {				\
	{ 1, IEEE80211_T_CCK, 1000, 27, 0x00, 130, 0 },			\
	{ 1, IEEE80211_T_CCK, 2000, 26, 0x04, 132, 1 },			\
	{ 1, IEEE80211_T_CCK, 5500, 25, 0x04, 139, 1 },			\
	{ 1, IEEE80211_T_CCK, 11000, 24, 0x04, 150, 1 } }		\
}

#define AR5K_RATES_11G { 12, {						\
	255, 255, 255, 255, 255, 255, 255, 255, 10, 8, 6, 4,		\
	11, 9, 7, 5, 255, 255, 255, 255, 255, 255, 255, 255,		\
	3, 2, 1, 0, 255, 255, 255, 255 }, {				\
	{ 1, IEEE80211_T_CCK, 1000, 27, 0x00, 2, 0 },			\
	{ 1, IEEE80211_T_CCK, 2000, 26, 0x04, 4, 1 },			\
	{ 1, IEEE80211_T_CCK, 5500, 25, 0x04, 11, 1 },			\
	{ 1, IEEE80211_T_CCK, 11000, 24, 0x04, 22, 1 },			\
	{ 0, IEEE80211_T_OFDM, 6000, 11, 0, 12, 4 },			\
	{ 0, IEEE80211_T_OFDM, 9000, 15, 0, 18, 4 },			\
	{ 1, IEEE80211_T_OFDM, 12000, 10, 0, 24, 6 },			\
	{ 1, IEEE80211_T_OFDM, 18000, 14, 0, 36, 6 },			\
	{ 1, IEEE80211_T_OFDM, 24000, 9, 0, 48, 8 },			\
	{ 1, IEEE80211_T_OFDM, 36000, 13, 0, 72, 8 },			\
	{ 1, IEEE80211_T_OFDM, 48000, 8, 0, 96, 8 },			\
	{ 1, IEEE80211_T_OFDM, 54000, 12, 0, 108, 8 } }			\
}

#define AR5K_RATES_TURBO { 8, {						\
	255, 255, 255, 255, 255, 255, 255, 255, 6, 4, 2, 0,		\
	7, 5, 3, 1, 255, 255, 255, 255, 255, 255, 255, 255,		\
	255, 255, 255, 255, 255, 255, 255, 255 }, {			\
	{ 1, IEEE80211_T_TURBO, 6000, 11, 0, 140, 0 },			\
	{ 1, IEEE80211_T_TURBO, 9000, 15, 0, 18, 0 },			\
	{ 1, IEEE80211_T_TURBO, 12000, 10, 0, 152, 2 },			\
	{ 1, IEEE80211_T_TURBO, 18000, 14, 0, 36, 2 },			\
	{ 1, IEEE80211_T_TURBO, 24000, 9, 0, 176, 4 },			\
	{ 1, IEEE80211_T_TURBO, 36000, 13, 0, 72, 4 },			\
	{ 1, IEEE80211_T_TURBO, 48000, 8, 0, 96, 4 },			\
	{ 1, IEEE80211_T_TURBO, 54000, 12, 0, 108, 4 } }		\
}

#define AR5K_RATES_XR { 12, {						\
	255, 3, 1, 255, 255, 255, 2, 0, 10, 8, 6, 4,			\
	11, 9, 7, 5, 255, 255, 255, 255, 255, 255, 255, 255,		\
	255, 255, 255, 255, 255, 255, 255, 255 }, {			\
	{ 1, IEEE80211_T_XR, 500, 7, 0, 129, 0 },			\
	{ 1, IEEE80211_T_XR, 1000, 2, 0, 139, 1 },			\
	{ 1, IEEE80211_T_XR, 2000, 6, 0, 150, 2 },			\
	{ 1, IEEE80211_T_XR, 3000, 1, 0, 150, 3 },			\
	{ 1, IEEE80211_T_OFDM, 6000, 11, 0, 140, 4 },			\
	{ 1, IEEE80211_T_OFDM, 9000, 15, 0, 18, 4 },			\
	{ 1, IEEE80211_T_OFDM, 12000, 10, 0, 152, 6 },			\
	{ 1, IEEE80211_T_OFDM, 18000, 14, 0, 36, 6 },			\
	{ 1, IEEE80211_T_OFDM, 24000, 9, 0, 176, 8 },			\
	{ 1, IEEE80211_T_OFDM, 36000, 13, 0, 72, 8 },			\
	{ 1, IEEE80211_T_OFDM, 48000, 8, 0, 96, 8 },			\
	{ 1, IEEE80211_T_OFDM, 54000, 12, 0, 108, 8 } }			\
}




static const HAL_RATE_TABLE ar5k_rt_11a = AR5K_RATES_11A;
static const HAL_RATE_TABLE ar5k_rt_11b = AR5K_RATES_11B;
static const HAL_RATE_TABLE ar5k_rt_11g = AR5K_RATES_11G;
static const HAL_RATE_TABLE ar5k_rt_turbo = AR5K_RATES_TURBO;
static const HAL_RATE_TABLE ar5k_rt_xr = AR5K_RATES_XR;


/*
 * Fills in the HAL structure and initialises the device
 */
struct ath_hal *
ath_hal_attach(device, sc, st, sh, status)
	u_int16_t device;
	HAL_SOFTC sc;
	HAL_BUS_TAG st;
	HAL_BUS_HANDLE sh;
	HAL_STATUS *status;
{
	u_int32_t ieee_regdomain;
	u_int32_t regdomain;
	struct ath_hal *hal = 0;
	ar5k_attach_t *attach = 0;
	u_int8_t mac[6];
	int i;

	*status = EINVAL;

	/*
	 * Call the chipset-dependent attach routine by device id
	 */
	for (i = 0; i < AR5K_ELEMENTS(ar5k_known_products); i++) {
		if (device == ar5k_known_products[i].device &&
		    ar5k_known_products[i].attach != 0)
			attach = ar5k_known_products[i].attach;
	}

	if (attach == 0) {
		*status = ENXIO;
		//("device not supported: 0x%04x\n", device);
		return (0);
	}

	if ((hal = (struct ath_hal *) malloc(sizeof(struct ath_hal))) == 0) {
		*status = ENOMEM;
		AR5K_PRINT("out of memory\n");
		return (0);
	}

	bzero(hal, sizeof(struct ath_hal));

	hal->ah_sc = sc;
	hal->ah_st = st;
	hal->ah_sh = sh;
	hal->ah_device = device;
	//hal->ah_sub_vendor = 0; /* XXX unknown?! */

	/*
	 * HAL information
	 */
	hal->ah_abi = HAL_ABI_VERSION;
	hal->ah_country_code = CTRY_DEFAULT;
	hal->ah_capabilities.cap_regdomain.reg_current = 0x10;
	hal->ah_op_mode = HAL_M_STA;
	hal->ah_radar.r_enabled = AR5K_TUNE_RADAR_ALERT;
	hal->ah_turbo = AH_FALSE;
	hal->ah_txpower.txp_tpc = AR5K_TUNE_TPC_TXPOWER;
	hal->ah_imr = 0;
	hal->ah_atim_window = 0;
	hal->ah_aifs = AR5K_TUNE_AIFS;
	hal->ah_cw_min = AR5K_TUNE_CWMIN;
	hal->ah_limit_tx_retries = AR5K_INIT_TX_RETRY;
	hal->ah_software_retry = AH_FALSE;
	hal->ah_ant_diversity = AR5K_TUNE_ANT_DIVERSITY;

	if ((attach)(device, hal, st, sh, status) == 0)
		goto failed;

#ifdef AR5K_DEBUG
	hal->ah_dump_state(hal);
#endif

	/*
	 * Get card capabilities, values, ...
	 */

	if (ar5k_eeprom_init(hal) != 0) {
		AR5K_PRINT("unable to init EEPROM\n");
		goto failed;
	}

	/* Set regulation domain */
	if ((regdomain =
		hal->ah_capabilities.cap_eeprom.ee_regdomain) != 0) {
		hal->ah_capabilities.cap_regdomain.reg_current =
		    ieee_regdomain = ar5k_regdomain_to_ieee(regdomain);
	} else {
		ieee_regdomain =
		    hal->ah_capabilities.cap_regdomain.reg_current;

		/* Try to write default regulation domain to EEPROM */
		ar5k_eeprom_regulation_domain(hal, AH_TRUE, &ieee_regdomain);
	}

	hal->ah_capabilities.cap_regdomain.reg_hw = ieee_regdomain;

	/* Get misc capabilities */
	//if (hal->ah_get_capabilities(hal) != AH_TRUE) {
	//	AR5K_PRINTF("unable to get device capabilities: 0x%04x\n",
	//	    device);
	//	goto failed;
	//}

	/* Get MAC address */
	if ((*status = ar5k_eeprom_read_mac(hal, mac)) != 0) {
		AR5K_PRINTF("unable to read address from EEPROM: 0x%04x\n",
		    device);
		goto failed;
	}

	//hal->ah_set_lladdr(hal, mac);

	/* Get rate tables */
	if (hal->ah_capabilities.cap_mode & HAL_MODE_11A)
		ar5k_rt_copy(&hal->ah_rt_11a, &ar5k_rt_11a);
	if (hal->ah_capabilities.cap_mode & HAL_MODE_11B)
		ar5k_rt_copy(&hal->ah_rt_11b, &ar5k_rt_11b);
	if (hal->ah_capabilities.cap_mode & HAL_MODE_11G)
		ar5k_rt_copy(&hal->ah_rt_11g, &ar5k_rt_11g);
	if (hal->ah_capabilities.cap_mode & HAL_MODE_TURBO)
		ar5k_rt_copy(&hal->ah_rt_turbo, &ar5k_rt_turbo);
	//if (hal->ah_capabilities.cap_mode & HAL_MODE_XR)
	//ar5k_rt_copy(&hal->ah_rt_xr, &ar5k_rt_xr);

	/* Initialize the gain optimization values */
	/* XXX
	if (hal->ah_radio == AR5K_AR5111) {
		hal->ah_gain.g_step_idx = ar5111_gain_opt.go_default;
		hal->ah_gain.g_step =
		    &ar5111_gain_opt.go_step[hal->ah_gain.g_step_idx];
		hal->ah_gain.g_low = 20;
		hal->ah_gain.g_high = 35;
		hal->ah_gain.g_active = 1;
	} else if (hal->ah_radio == AR5K_AR5112) {
		hal->ah_gain.g_step_idx = ar5112_gain_opt.go_default;
		hal->ah_gain.g_step =
		    &ar5111_gain_opt.go_step[hal->ah_gain.g_step_idx];
		hal->ah_gain.g_low = 20;
		hal->ah_gain.g_high = 85;
		hal->ah_gain.g_active = 1;
	}
	*/

	*status = HAL_OK;

	return (hal);

 failed:
	free(hal);
	return (0);
}
