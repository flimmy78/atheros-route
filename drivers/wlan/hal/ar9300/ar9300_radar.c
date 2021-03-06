/*
 * Copyright (c) 2008-2010, Atheros Communications Inc.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */


#include "opt_ah.h"

#ifdef AH_SUPPORT_AR9300

#include "ah.h"
#include "ah_desc.h"
#include "ah_internal.h"

#include "ar9300/ar9300.h"
#include "ar9300/ar9300phy.h"
#include "ar9300/ar9300reg.h"

/*
 * Default 5413/9300 radar phy parameters
 * Values adjusted to fix EV76432/EV76320
 */
#define AR9300_DFS_FIRPWR   -28
#define AR9300_DFS_RRSSI    0
#define AR9300_DFS_HEIGHT   10
#define AR9300_DFS_PRSSI    6 
#define AR9300_DFS_INBAND   8
#define AR9300_DFS_RELPWR   8
#define AR9300_DFS_RELSTEP  12
#define AR9300_DFS_MAXLEN   255
#define AR9300_DFS_PRSSI_CAC 10 

/*
 *  make sure that value matches value in ar9300_osprey_2p2_mac_core[][2]
 *  for register 0x1040 to 0x104c
*/
#define AR9300_DEFAULT_DIFS     0x002ffc0f

struct dfs_pulse ar9300_etsi_radars[] = {

    /* for short pulses, RSSI threshold should be smaller than
 * Kquick-drop. The chip has only one chance to drop the gain which
 * will be reported as the estimated RSSI */

    /* TYPE staggered pulse */
    /* 0.8-2us, 2-3 bursts,300-400 PRF, 10 pulses each */
    {30,  2,  300,  400, 2, 30,  3,  0,  5, 15, 0,   0, 31},   /* Type 5*/
    /* 0.8-2us, 2-3 bursts, 400-1200 PRF, 15 pulses each */
    {30,  2,  400, 1200, 2, 30,  7,  0,  5, 15, 0,   0, 32},   /* Type 6 */

    /* constant PRF based */
    /* 0.8-5us, 200  300 PRF, 10 pulses */
    {10, 5,   200,  300, 0, 24,  5,  0,  8, 15, 0,   0, 40},   /* Type 1 */
    {10, 5,   300,  400, 0, 24,  5,  0,  8, 15, 0,   0, 33},   /* Type 1 */
    {10, 5,   400,  600, 0, 24,  5,  0,  8, 15, 0,   0, 37},   /* Type 1 */
    {10, 5,   600,  800, 0, 24,  5,  0,  8, 15, 0,   0, 38},   /* Type 1 */
    {10, 5,   800, 1000, 0, 24,  5,  0,  8, 15, 0,   0, 39},   /* Type 1 */
//  {10, 5,   200, 1000, 0, 24,  5,  0,  8, 15, 0,   0, 33},   

    /* 0.8-15us, 200-1600 PRF, 15 pulses */
    {15, 15,  200, 1600, 0, 24, 8,  0, 18, 24, 0,   0, 34},    /* Type 2 */

    /* 0.8-15us, 2300-4000 PRF, 25 pulses*/
    {25, 15, 2300, 4000,  0, 24, 10, 0, 18, 24, 0,   0, 35},   /* Type 3 */

    /* 20-30us, 2000-4000 PRF, 20 pulses*/
    {20, 30, 2000, 4000, 0, 24, 8, 19, 33, 24, 0,   0, 36},    /* Type 4 */
};

/* The following are for FCC Bin 1-4 pulses */
struct dfs_pulse ar9300_fcc_radars[] = {

    /* following two filters are specific to Japan/MKK4 */
//    {18,  1,  720,  720, 1,  6,  6,  0,  1, 18,  0, 3, 17}, // 1389 +/- 6 us
//    {18,  4,  250,  250, 1, 10,  5,  1,  6, 18,  0, 3, 18}, // 4000 +/- 6 us
//    {18,  5,  260,  260, 1, 10,  6,  1,  6, 18,  0, 3, 19}, // 3846 +/- 7 us
    {18,  1,  720,  720, 0,  6,  6,  0,  1, 18,  0, 3, 17}, // 1389 +/- 6 us
    {18,  4,  250,  250, 0, 10,  5,  1,  6, 18,  0, 3, 18}, // 4000 +/- 6 us
    /*
      Lower the detection threshold for Japan W53 from 6 to 4.
      When we test for radar by using 11n mode (with aggregation on at
      50% channel loading in veriwave), fewer radar pulse are reported by 
      the hardware.
      If the test is perfromed in 11ag mode of veriwave, higher value works fine.
    */
      

    /* The next one if Japan W53, type 1. This is same as FCC type 1 */
    /* However, Japan W53 is tested at a 50% load comapared to 17% for FCC */
    /* There are very few pulses reported at that load. So we maintain a */
    /* seperate entry */
      
    {18,  1,  700,  700, 0,  6,  4,  0,  1, 18,  0, 3, 19}, 
    {18,  5,  260,  260, 0, 10,  4,  1,  6, 18,  0, 3, 20}, // 3846 +/- 7 us (Japan W53 type 2)
    {18,  5,  260,  260, 1, 10,  4,  1,  6, 18,  0, 3, 21}, // 3846 +/- 7 us (Japan W53 type 2)


    /* following filters are common to both FCC and JAPAN */

    // FCC TYPE 1
    // {18,  1,  325, 1930, 0,  6,  7,  0,  1, 18,  0, 3,  0}, // 518 to 3066
    {18,  1,  700, 700, 0,  6,  5,  0,  1, 18,  0, 3,  8}, 
    {18,  1,  350, 350, 0,  6,  5,  0,  1, 18,  0, 3,  0}, 
							   

    // FCC TYPE 6							   
    // {9,   1, 3003, 3003, 1,  7,  5,  0,  1, 18,  0, 0,  1}, // 333 +/- 7 us
    {9,   1, 3003, 3003, 1,  7,  5,  0,  1, 18,  0, 0,  1},

    // FCC TYPE 2	
    {23, 5, 4347, 6666, 0, 18, 11,  0,  7, 22,  0, 3,  2}, 

    // FCC TYPE 3
    {18, 10, 2000, 5000, 0, 23,  8,  6, 13, 22,  0, 3, 5},

    // FCC TYPE 4
    {16, 15, 2000, 5000, 0, 25,  7, 11, 23, 22,  0, 3, 11}, 

};

struct dfs_bin5pulse ar9300_bin5pulses[] = {
        {2, 28, 105, 12, 22, 5},
};

/*
 * Find the internal HAL channel corresponding to the
 * public HAL channel specified in c
 */

static HAL_CHANNEL_INTERNAL *
getchannel(struct ath_hal *ah, const HAL_CHANNEL *c)
{
#define CHAN_FLAGS    (CHANNEL_ALL | CHANNEL_HALF | CHANNEL_QUARTER)
    HAL_CHANNEL_INTERNAL *base, *cc;
    int flags = c->channelFlags & CHAN_FLAGS;
    int n, lim;

    /*
     * Check current channel to avoid the lookup.
     */
    cc = AH_PRIVATE(ah)->ah_curchan;
    if (cc != AH_NULL && cc->channel == c->channel &&
        (cc->channelFlags & CHAN_FLAGS) == flags) {
        return cc;
    }

    /* binary search based on known sorting order */
    base = AH_TABLES(ah)->ah_channels;
    n = AH_PRIVATE(ah)->ah_nchan;
    /* binary search based on known sorting order */
    for (lim = n; lim != 0; lim >>= 1) {
        int d;
        cc = &base[lim >> 1];
        d = c->channel - cc->channel;
        if (d == 0) {
            if ((cc->channelFlags & CHAN_FLAGS) == flags) {
                return cc;
            }
            d = flags - (cc->channelFlags & CHAN_FLAGS);
        }
        HDPRINTF(ah, HAL_DBG_DFS, "%s: channel %u/0x%x d %d\n", __func__,
                cc->channel, cc->channelFlags, d);
        if (d > 0) {
            base = cc + 1;
            lim--;
        }
    }
    HDPRINTF(ah, HAL_DBG_DFS, "%s: no match for %u/0x%x\n",
            __func__, c->channel, c->channelFlags);
    return AH_NULL;
#undef CHAN_FLAGS
}

/* Check the internal channel list to see if the desired channel
 * is ok to release from the NOL.  If not, then do nothing.  If so,
 * mark the channel as clear and reset the internal tsf time
 */

void
ar9300CheckDfs(struct ath_hal *ah, HAL_CHANNEL *chan)
{
    HAL_CHANNEL_INTERNAL *ichan = AH_NULL;

    ichan = getchannel(ah, chan);
    if (ichan == AH_NULL) {
        return;
    }
    if (!(ichan->privFlags & CHANNEL_INTERFERENCE)) {
        return;
    }

    ichan->privFlags &= ~CHANNEL_INTERFERENCE;
    ichan->dfsTsf = 0;
}

/*
 * This function marks the channel as having found a dfs event
 * It also marks the end time that the dfs event should be cleared
 * If the channel is already marked, then tsf end time can only
 * be increased
 */

void
ar9300DfsFound(struct ath_hal *ah, HAL_CHANNEL *chan, u_int64_t nolTime)
{
    HAL_CHANNEL_INTERNAL *ichan;

    ichan = getchannel(ah, chan);
    if (ichan == AH_NULL) {
        return;
    }
    if (!(ichan->privFlags & CHANNEL_INTERFERENCE)) {
        ichan->dfsTsf = ar9300GetTsf64(ah);
    }
    ichan->dfsTsf += nolTime;
    ichan->privFlags |= CHANNEL_INTERFERENCE;
    chan->privFlags |= CHANNEL_INTERFERENCE;
}

/*
 * Enable radar detection and set the radar parameters per the
 * values in pe
 */

void
ar9300EnableDfs(struct ath_hal *ah, HAL_PHYERR_PARAM *pe)
{
    u_int32_t val;
    struct ath_hal_private  *ahp = AH_PRIVATE(ah);
    HAL_CHANNEL_INTERNAL *ichan = ahp->ah_curchan;
    struct ath_hal_9300 *ah9300 = AH9300(ah);
    int reg_writes = 0;

    val = OS_REG_READ(ah, AR_PHY_RADAR_0);
    if (pe->pe_firpwr != HAL_PHYERR_PARAM_NOVAL) {
        val &= ~AR_PHY_RADAR_0_FIRPWR;
        val |= SM(pe->pe_firpwr, AR_PHY_RADAR_0_FIRPWR);
    }
    if (pe->pe_rrssi != HAL_PHYERR_PARAM_NOVAL) {
        val &= ~AR_PHY_RADAR_0_RRSSI;
        val |= SM(pe->pe_rrssi, AR_PHY_RADAR_0_RRSSI);
    }
    if (pe->pe_height != HAL_PHYERR_PARAM_NOVAL) {
        val &= ~AR_PHY_RADAR_0_HEIGHT;
        val |= SM(pe->pe_height, AR_PHY_RADAR_0_HEIGHT);
    }
    if (pe->pe_prssi != HAL_PHYERR_PARAM_NOVAL) {
        val &= ~AR_PHY_RADAR_0_PRSSI;

        if (AR_SREV_AR9580(ah)) {
            if (ah->ah_use_cac_prssi) {
                val |= SM(AR9300_DFS_PRSSI_CAC, AR_PHY_RADAR_0_PRSSI);
            } else {
                val |= SM(pe->pe_prssi, AR_PHY_RADAR_0_PRSSI);
            }
        } else {
            val |= SM(pe->pe_prssi, AR_PHY_RADAR_0_PRSSI);
        }
    }
    if (pe->pe_inband != HAL_PHYERR_PARAM_NOVAL) {
        val &= ~AR_PHY_RADAR_0_INBAND;
        val |= SM(pe->pe_inband, AR_PHY_RADAR_0_INBAND);
    }

    /*Enable FFT data*/
    val |= AR_PHY_RADAR_0_FFT_ENA;

    OS_REG_WRITE(ah, AR_PHY_RADAR_0, val | AR_PHY_RADAR_0_ENA);

    val = OS_REG_READ(ah, AR_PHY_RADAR_1);
    val |= (AR_PHY_RADAR_1_MAX_RRSSI | AR_PHY_RADAR_1_BLOCK_CHECK);

    if (pe->pe_maxlen != HAL_PHYERR_PARAM_NOVAL) {
        val &= ~AR_PHY_RADAR_1_MAXLEN;
        val |= SM(pe->pe_maxlen, AR_PHY_RADAR_1_MAXLEN);
    }

    OS_REG_WRITE(ah, AR_PHY_RADAR_1, val);

    if (ath_hal_getcapability(ah, HAL_CAP_EXT_CHAN_DFS, 0, 0) == HAL_OK) {
        if (IS_CHAN_HT40(ichan)) {
            /*Enable extension channel radar detection*/
            val = OS_REG_READ(ah, AR_PHY_RADAR_EXT);
            OS_REG_WRITE(ah, AR_PHY_RADAR_EXT, val | AR_PHY_RADAR_EXT_ENA);
        } else {
            /*HT20 mode, disable extension channel radar detect*/
            val = OS_REG_READ(ah, AR_PHY_RADAR_EXT);
            OS_REG_WRITE(ah, AR_PHY_RADAR_EXT, val & ~AR_PHY_RADAR_EXT_ENA);
        }
    }

    if (pe->pe_relstep != HAL_PHYERR_PARAM_NOVAL) {
        val = OS_REG_READ(ah, AR_PHY_RADAR_1);
        val &= ~AR_PHY_RADAR_1_RELSTEP_THRESH;
        val |= SM(pe->pe_relstep, AR_PHY_RADAR_1_RELSTEP_THRESH);
        OS_REG_WRITE(ah, AR_PHY_RADAR_1, val);
    }
    if (pe->pe_relpwr != HAL_PHYERR_PARAM_NOVAL) {
        val = OS_REG_READ(ah, AR_PHY_RADAR_1);
        val &= ~AR_PHY_RADAR_1_RELPWR_THRESH;
        val |= SM(pe->pe_relpwr, AR_PHY_RADAR_1_RELPWR_THRESH);
        OS_REG_WRITE(ah, AR_PHY_RADAR_1, val);
    }
    /*
        apply DFS postamble array from INI
        column 0 is register ID, column 1 is  HT20 value, colum2 is HT40 value
    */

    if (AR_SREV_AR9580(ah) || AR_SREV_WASP(ah) || AR_SREV_OSPREY_22(ah)) {
        REG_WRITE_ARRAY(&ah9300->ah_ini_dfs,IS_CHAN_HT40(ichan)? 2:1, reg_writes);
    }
}

/*
 * Get the radar parameter values and return them in the pe
 * structure
 */

void
ar9300GetDfsThresh(struct ath_hal *ah, HAL_PHYERR_PARAM *pe)
{
    u_int32_t val,temp;

    val = OS_REG_READ(ah, AR_PHY_RADAR_0);

    temp = MS(val,AR_PHY_RADAR_0_FIRPWR);
    temp |= 0xFFFFFF80;
    pe->pe_firpwr = temp;
    pe->pe_rrssi = MS(val, AR_PHY_RADAR_0_RRSSI);
    pe->pe_height =  MS(val, AR_PHY_RADAR_0_HEIGHT);
    pe->pe_prssi = MS(val, AR_PHY_RADAR_0_PRSSI);
    pe->pe_inband = MS(val, AR_PHY_RADAR_0_INBAND);

    val = OS_REG_READ(ah, AR_PHY_RADAR_1);
    temp = val & AR_PHY_RADAR_1_RELPWR_ENA;
    pe->pe_relpwr = MS(val, AR_PHY_RADAR_1_RELPWR_THRESH);
    if (temp) {
        pe->pe_relpwr |= HAL_PHYERR_PARAM_ENABLE;
    }
    temp = val & AR_PHY_RADAR_1_RELSTEP_CHECK;
    pe->pe_relstep = MS(val, AR_PHY_RADAR_1_RELSTEP_THRESH);
    if (temp) {
        pe->pe_relstep |= HAL_PHYERR_PARAM_ENABLE;
    }
    pe->pe_maxlen = MS(val, AR_PHY_RADAR_1_MAXLEN);
}

HAL_BOOL
ar9300RadarWait(struct ath_hal *ah, HAL_CHANNEL *chan)
{
    struct ath_hal_private *ahp = AH_PRIVATE(ah);

    if (!ahp->ah_curchan) {
        return AH_TRUE;
    }

    /*
     * Rely on the upper layers to determine that we have spent
     * enough time waiting.
     */
    chan->channel = ahp->ah_curchan->channel;
    chan->channelFlags = ahp->ah_curchan->channelFlags;
    chan->maxRegTxPower = ahp->ah_curchan->maxRegTxPower;

    ahp->ah_curchan->privFlags |= CHANNEL_DFS_CLEAR;
    chan->privFlags  = ahp->ah_curchan->privFlags;
    return AH_FALSE;

}

struct dfs_pulse *
ar9300GetDfsRadars(
    struct ath_hal *ah,
    u_int32_t dfsdomain,
    int *numradars,
    struct dfs_bin5pulse **bin5pulses,
    int *numb5radars,
    HAL_PHYERR_PARAM *pe)
{
#define N(a)    (sizeof(a) / sizeof(a[0]))
    struct dfs_pulse *dfs_radars = AH_NULL;
    switch (dfsdomain) {
    case DFS_FCC_DOMAIN:
        dfs_radars = &ar9300_fcc_radars[5];
        *numradars = N(ar9300_fcc_radars)-5;
        *bin5pulses = &ar9300_bin5pulses[0];
        *numb5radars = N(ar9300_bin5pulses);
        HDPRINTF(ah, HAL_DBG_DFS, "%s: DFS_FCC_DOMAIN_9300\n", __func__);
        break;
    case DFS_ETSI_DOMAIN:
        dfs_radars = &ar9300_etsi_radars[0];
        *numradars = N(ar9300_etsi_radars);
        *bin5pulses = &ar9300_bin5pulses[0];
        *numb5radars = N(ar9300_bin5pulses);
        HDPRINTF(ah, HAL_DBG_DFS, "%s: DFS_ETSI_DOMAIN_9300\n", __func__);
        break;
    case DFS_MKK4_DOMAIN:
        dfs_radars = &ar9300_fcc_radars[0];
        *numradars = N(ar9300_fcc_radars);
        *bin5pulses = &ar9300_bin5pulses[0];
        *numb5radars = N(ar9300_bin5pulses);
        HDPRINTF(ah, HAL_DBG_DFS, "%s: DFS_MKK4_DOMAIN_9300\n", __func__);
        break;
    default:
        HDPRINTF(ah, HAL_DBG_DFS, "%s: no domain\n", __func__);
        return AH_NULL;
    }
    /* Set the default phy parameters per chip */
    pe->pe_firpwr = AR9300_DFS_FIRPWR;
    pe->pe_rrssi = AR9300_DFS_RRSSI;
    pe->pe_height = AR9300_DFS_HEIGHT;
    pe->pe_prssi = AR9300_DFS_PRSSI;

    /*
        we have an issue with PRSSI.
        For normal operation we use AR9300_DFS_PRSSI, which is set to 6.
        Please refer to EV91563, 94164.
        However, this causes problem during CAC as no radar is detected
        during that period with PRSSI=6. Only PRSSI= 10 seems to fix this.
        We use this flag to keep track of change in PRSSI.
    */

    ah->ah_use_cac_prssi = 0;
	
    pe->pe_inband = AR9300_DFS_INBAND;
    pe->pe_relpwr = AR9300_DFS_RELPWR;
    pe->pe_relstep = AR9300_DFS_RELSTEP;
    pe->pe_maxlen = AR9300_DFS_MAXLEN;
    return dfs_radars;
#undef N
}

HAL_CHANNEL *ar9300GetExtensionChannel(struct ath_hal *ah)
{
    struct ath_hal_private  *ahp = AH_PRIVATE(ah);
    struct ath_hal_private_tables  *aht = AH_TABLES(ah);
    int i = 0;

    HAL_CHANNEL_INTERNAL *ichan = AH_NULL;
    CHAN_CENTERS centers;

    ichan = ahp->ah_curchan;
    ar9300GetChannelCenters(ah, ichan, &centers);
    if (centers.ctl_center == centers.ext_center) {
        return AH_NULL;
    }
    for (i = 0; i < ahp->ah_nchan; i++) {
        ichan = &aht->ah_channels[i];
        if (ichan->channel == centers.ext_center) {
            return (HAL_CHANNEL*)ichan;
        }
    }
    return AH_NULL;
}


HAL_BOOL ar9300IsFastClockEnabled(struct ath_hal *ah)
{
    struct ath_hal_private *ahp = AH_PRIVATE(ah);

    if (IS_5GHZ_FAST_CLOCK_EN(ah, ahp->ah_curchan)) {
        return AH_TRUE;
    }
    return AH_FALSE;
}

void ar9300_adjust_difs(struct ath_hal *ah, u_int32_t val)
{
    if (val == 0) {
	/*
         * These are default values for these registers
         * make sure that they match values in ar9300_osprey_2p2_mac_core[][2]
         * in ar9300_osprey22.ini
         */

        ah->ah_fccaifs = 0;
        HDPRINTF(ah, HAL_DBG_DFS, "%s: set DIFS to default\n", __func__);
        OS_REG_WRITE(ah, AR_DLCL_IFS(0), AR9300_DEFAULT_DIFS);
        OS_REG_WRITE(ah, AR_DLCL_IFS(1), AR9300_DEFAULT_DIFS);
        OS_REG_WRITE(ah, AR_DLCL_IFS(2), AR9300_DEFAULT_DIFS);
        OS_REG_WRITE(ah, AR_DLCL_IFS(3), AR9300_DEFAULT_DIFS);
    } else {
	/*
         * These are values from George Lai and are specific to
         * FCC domain. They are yet to be determined for other domains. 
         */
        ah->ah_fccaifs = 1;
        HDPRINTF(ah, HAL_DBG_DFS, "%s: set DIFS to default\n", __func__);
        OS_REG_WRITE(ah, AR_DLCL_IFS(0), 0x05fffc0f);
        OS_REG_WRITE(ah, AR_DLCL_IFS(1), 0x05f0fc0f);
        OS_REG_WRITE(ah, AR_DLCL_IFS(2), 0x05f03c07);
        OS_REG_WRITE(ah, AR_DLCL_IFS(3), 0x05f01c03);
    }
}

HAL_BOOL
ar9300_handle_radar_bb_panic(struct ath_hal *ah)
{
    u_int32_t status;
    u_int32_t val;   
       
    status = AH_PRIVATE(ah)->ah_bbPanicLastStatus;
   
    if ( status == 0x04000539 ) { 
        /* recover from this BB panic without reset*/
        /* set AR9300_DFS_FIRPWR to -1 */
        val = OS_REG_READ(ah, AR_PHY_RADAR_0);
        val &= (~AR_PHY_RADAR_0_FIRPWR);
        val |= SM( 0x7f, AR_PHY_RADAR_0_FIRPWR);
        OS_REG_WRITE(ah, AR_PHY_RADAR_0, val);
        OS_DELAY(1);
        /* set AR9300_DFS_FIRPWR to its default value */
        val = OS_REG_READ(ah, AR_PHY_RADAR_0);
        val &= ~AR_PHY_RADAR_0_FIRPWR;
        val |= SM( AR9300_DFS_FIRPWR, AR_PHY_RADAR_0_FIRPWR);
        OS_REG_WRITE(ah, AR_PHY_RADAR_0, val);
        return AH_TRUE;
    } else if (status == 0x0400000a) {
        /* EV 92527 : reset required if we see this signature */
        HDPRINTF(ah, HAL_DBG_DFS, "%s: BB Panic -- 0x0400000a\n", __func__);
        return AH_FALSE;
    } else if (status == 0x1300000a) {
        /* EV92527: we do not need a reset if we see this signature */
        HDPRINTF(ah, HAL_DBG_DFS, "%s: BB Panic -- 0x1300000a\n", __func__);
        return AH_TRUE;
    } else if (AR_SREV_WASP(ah) && (status == 0x04000409)) {
        return AH_TRUE;
    } else {
        return AH_FALSE;
    }
}

/*
    function to adjust PRSSI value for CAC problem

*/
void
ar9300_dfs_cac_war(struct ath_hal *ah, u_int32_t start)
{
    u_int32_t val;

    if (AR_SREV_AR9580(ah) || AR_SREV_OSPREY_22(ah)) {
        val = OS_REG_READ(ah, AR_PHY_RADAR_0);
        if (start) {
            val &= ~AR_PHY_RADAR_0_PRSSI;
            val |= SM(AR9300_DFS_PRSSI_CAC, AR_PHY_RADAR_0_PRSSI); 
        } else {
            val &= ~AR_PHY_RADAR_0_PRSSI;
            val |= SM(AR9300_DFS_PRSSI, AR_PHY_RADAR_0_PRSSI);
        }
        OS_REG_WRITE(ah, AR_PHY_RADAR_0, val | AR_PHY_RADAR_0_ENA);
        ah->ah_use_cac_prssi = start;
    }
}

#endif
