/*
 *  Copyright (c) 2008 Atheros Communications Inc.
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
#include "ieee80211_var.h"
#include <ieee80211_channel.h>
#include "ieee80211_sme_api.h"
#include "ieee80211_sm.h"
#include "ieee80211_power.h"

#if UMAC_SUPPORT_RESMGR

static const char *state_to_name[] = {
     "INIT",
     "AWAKE",
     "FULL_SLEEP",
     "NETWORK_SLEEP",
     "FAKE_SLEEP",
     "PSPOLL"
};

#include <ieee80211_resmgr_priv.h>


static void ieee80211_vap_iter_pm_state(void *arg, struct ieee80211vap *vap)
{
    ieee80211_resmgr_vap_pmstate_t *combined_state = (ieee80211_resmgr_vap_pmstate_t *)arg;
    ieee80211_resmgr_vap_priv_t resmgr_vap = ieee80211vap_get_resmgr(vap);

    if (resmgr_vap && resmgr_vap->pm_state > *combined_state) {
        *combined_state = resmgr_vap->pm_state;
    }
}

static void ieee80211_resmgr_reset_power_state(ieee80211_resmgr_t resmgr)
{
    int pm_state;
    struct ieee80211com *ic = resmgr->ic;
    ieee80211_resmgr_vap_pmstate_t combined_state = VAP_PM_FULL_SLEEP;

    IEEE80211_RESMGR_LOCK(resmgr);

    if (resmgr->is_scan_in_progress) {
        combined_state = VAP_PM_ACTIVE;
    } else {
        wlan_iterate_vap_list(ic, ieee80211_vap_iter_pm_state, (void *)&combined_state);
    }

    switch(combined_state) {
    case VAP_PM_FULL_SLEEP:
        pm_state = IEEE80211_PWRSAVE_FULL_SLEEP;
        break;
    case VAP_PM_NW_SLEEP:
        pm_state = IEEE80211_PWRSAVE_NETWORK_SLEEP;
        break;
    default:
        pm_state = IEEE80211_PWRSAVE_AWAKE;
        break;
    }
    if (pm_state != resmgr->pm_state) {
        IEEE80211_DPRINTF_IC(ic, IEEE80211_MSG_POWER, IEEE80211_VERBOSE_LOUD,
                "%s: changing chip power state from %s to %s \n",
                __func__, state_to_name[resmgr->pm_state],
                state_to_name[pm_state]);
        resmgr->pm_state = pm_state;
        ic->ic_pwrsave_set_state(ic, pm_state);
    }
    IEEE80211_RESMGR_UNLOCK(resmgr);
}

/*
 * CanPause Timeout callback. 
 */
static OS_TIMER_FUNC(ieee80211_resmgr_canpause_timeout_callback)
{
    ieee80211_resmgr_t resmgr;
    struct ieee80211_resmgr_sm_event event;

    OS_GET_TIMER_ARG(resmgr, ieee80211_resmgr_t);
    event.vap = NULL;
    event.chan = NULL; 

    /* Post event to ResMgr SM */
    (void)ieee80211_resmgr_sm_dispatch(resmgr, IEEE80211_RESMGR_EVENT_VAP_CANPAUSE_TIMER, &event);
}

/*********************************************
 ****** Resource Manager API definitions
 *********************************************/
/*
 * Registered VAP event/notification handler
 */
static void ieee80211_resmgr_vap_evhandler(ieee80211_vap_t vap, ieee80211_vap_event *event, void *arg)
{
    ieee80211_resmgr_t resmgr = (ieee80211_resmgr_t)arg;
    struct ieee80211_resmgr_sm_event evt;
    int retVal=EOK;
    ieee80211_resmgr_vap_priv_t resmgr_vap = ieee80211vap_get_resmgr(vap);

    evt.vap = vap;
    evt.chan = NULL; 

    switch(event->type) {
    case IEEE80211_VAP_UP:
        retVal = ieee80211_resmgr_sm_dispatch(resmgr, IEEE80211_RESMGR_EVENT_VAP_UP, &evt);
        break;

    case IEEE80211_VAP_ACTIVE:
        resmgr_vap->pm_state = VAP_PM_ACTIVE;
        break;

    case IEEE80211_VAP_PAUSED:
    case IEEE80211_VAP_STANDBY:
        retVal = ieee80211_resmgr_sm_dispatch(resmgr, IEEE80211_RESMGR_EVENT_VAP_PAUSE_COMPLETE, &evt);
        break;

    case IEEE80211_VAP_PAUSE_FAIL:
        retVal = ieee80211_resmgr_sm_dispatch(resmgr, IEEE80211_RESMGR_EVENT_VAP_PAUSE_FAIL, &evt);
        break;

    case IEEE80211_VAP_UNPAUSED:
    case IEEE80211_VAP_RESUMED:
        retVal = ieee80211_resmgr_sm_dispatch(resmgr, IEEE80211_RESMGR_EVENT_VAP_RESUME_COMPLETE, &evt);
        break;

    case IEEE80211_VAP_CSA_COMPLETE:
        retVal = ieee80211_resmgr_sm_dispatch(resmgr, IEEE80211_RESMGR_EVENT_CSA_COMPLETE, &evt);
        break;

    case IEEE80211_VAP_NETWORK_SLEEP:
        resmgr_vap->pm_state = VAP_PM_NW_SLEEP;
        break;

    case IEEE80211_VAP_FULL_SLEEP:
        retVal = ieee80211_resmgr_sm_dispatch(resmgr, IEEE80211_RESMGR_EVENT_VAP_STOPPED, &evt);
        resmgr_vap->pm_state = VAP_PM_FULL_SLEEP;
        break;

    case IEEE80211_VAP_STOPPING:
        /* treat it like vap stopped */
        retVal = ieee80211_resmgr_sm_dispatch(resmgr, IEEE80211_RESMGR_EVENT_VAP_STOPPED, &evt);
        break;
    default:
        break;
    }

    ieee80211_resmgr_reset_power_state(resmgr);
}

/*
 * Registered SCAN event/nodification handler
 */
static void ieee80211_resmgr_scan_evhandler(wlan_if_t vaphandle, ieee80211_scan_event *event, void *arg)  
{
    ieee80211_resmgr_t resmgr= (ieee80211_resmgr_t) arg;

    // Event IEEE80211_SCAN_DEQUEUED must be ignored
    if ((event->type == IEEE80211_SCAN_STARTED)   ||
        (event->type == IEEE80211_SCAN_RESTARTED) ||
        (event->type == IEEE80211_SCAN_COMPLETED) || 
        (event->type == IEEE80211_SCAN_PREEMPTED)) {
        if ((event->type == IEEE80211_SCAN_COMPLETED) || 
            (event->type == IEEE80211_SCAN_PREEMPTED)) {
            resmgr->is_scan_in_progress=0;
        } else {
            resmgr->is_scan_in_progress=1;
        }
        ieee80211_resmgr_reset_power_state(resmgr);
    }
}

/*
 * Register a resmgr notification handler.
 */
int 
ieee80211_resmgr_register_notification_handler(ieee80211_resmgr_t resmgr, 
                                               ieee80211_resmgr_notification_handler notificationhandler,
                                               void *arg)
{
    int i;

    /* unregister if there exists one already */
    ieee80211_resmgr_unregister_notification_handler(resmgr, notificationhandler,arg);
    spin_lock(&resmgr->rm_handler_lock);
    for (i=0; i<IEEE80211_MAX_RESMGR_EVENT_HANDLERS; ++i) {
        if (resmgr->notif_handler[i] == NULL) {
            resmgr->notif_handler[i] = notificationhandler;
            resmgr->notif_handler_arg[i] = arg;
            spin_unlock(&resmgr->rm_handler_lock);
            return 0;
        }
    }
    spin_unlock(&resmgr->rm_handler_lock);
    return -ENOMEM;
}

/*
 * Unregister a resmgr event handler.
 */
int ieee80211_resmgr_unregister_notification_handler(ieee80211_resmgr_t resmgr, 
                                                     ieee80211_resmgr_notification_handler handler,
                                                     void  *arg)
{
    int i;
    spin_lock(&resmgr->rm_handler_lock);
    for (i=0; i<IEEE80211_MAX_RESMGR_EVENT_HANDLERS; ++i) {
        if (resmgr->notif_handler[i] == handler && resmgr->notif_handler_arg[i] == arg ) {
            resmgr->notif_handler[i] = NULL;
            resmgr->notif_handler_arg[i] = NULL;
            spin_unlock(&resmgr->rm_handler_lock);
            return 0;
        }
    }
    spin_unlock(&resmgr->rm_handler_lock);
    return -EEXIST;
}


/*
 * vap attach. (VAP Module)
 */
void ieee80211_resmgr_vattach(ieee80211_resmgr_t resmgr, ieee80211_vap_t vap)
{
    int retval;
    ieee80211_resmgr_vap_priv_t resmgr_vap;

    /* Allocate and store ResMgr private data in the VAP structure */
    resmgr_vap = (ieee80211_resmgr_vap_priv_t) OS_MALLOC(vap->iv_ic->ic_osdev, 
                                                         sizeof(struct ieee80211_resmgr_vap_priv),
                                                         0);
    ASSERT(resmgr_vap != NULL);
    if (!resmgr_vap)
        return;
    OS_MEMZERO(resmgr_vap, sizeof(struct ieee80211_resmgr_vap_priv));

    resmgr_vap->state = VAP_STOPPED;
    resmgr_vap->resmgr = resmgr;

    /* Setup Off-Channel Scheduler data in the ResMgr private data */
    ieee80211_resmgr_oc_sched_vattach(resmgr, vap, resmgr_vap);
    
    /* Register an event handler with the VAP module */
    retval = ieee80211_vap_register_event_handler(vap, ieee80211_resmgr_vap_evhandler, (void *)resmgr);
    ASSERT(retval == 0);

    ieee80211vap_set_resmgr(vap,resmgr_vap);
}

/*
 * vap detach. (VAP Module)
 */
void ieee80211_resmgr_vdetach(ieee80211_resmgr_t resmgr, ieee80211_vap_t vap)
{
    ieee80211_resmgr_vap_priv_t resmgr_vap = ieee80211vap_get_resmgr(vap);
    
    /* Unregister event handler with VAP module */
    ieee80211_vap_unregister_event_handler(vap, ieee80211_resmgr_vap_evhandler, (void *)resmgr);

    /* Cleanup Off-Channel Scheduler data in the ResMgr private data */
    ieee80211_resmgr_oc_sched_vdetach(resmgr, resmgr_vap);
    
    /* Clear and free ResMgr private data from VAP */
    ieee80211vap_set_resmgr(vap,NULL);
    OS_FREE(resmgr_vap);
}

/*
 * VAP start request. (VAP Module)
 */
int ieee80211_resmgr_vap_start(ieee80211_resmgr_t resmgr,
                               ieee80211_vap_t vap, 
                               struct ieee80211_channel *chan,
                               u_int16_t reqid,
                               u_int16_t max_start_time)
{
    struct ieee80211_resmgr_sm_event event;
    ieee80211_resmgr_vap_priv_t resmgr_vap = ieee80211vap_get_resmgr(vap);
    event.vap = vap;
    event.chan = chan;
    event.max_time = max_start_time;
    resmgr_vap->pm_state = VAP_PM_ACTIVE;       
   
    /* Update chip power mgmt state */
    ieee80211_resmgr_reset_power_state(resmgr);

    /* Post event to ResMgr SM */
    if (ieee80211_resmgr_sm_dispatch(resmgr, IEEE80211_RESMGR_EVENT_VAP_START_REQUEST, &event) == EOK) {
        return EBUSY;  /* processing req asynchronously */
    } else {
        return EOK;    /* caller can change channel */
    }
}

/*
 * Off channel request. (Scan Module)
 */
int ieee80211_resmgr_request_offchan(ieee80211_resmgr_t resmgr,
                                     struct ieee80211_channel *chan,
                                     u_int16_t reqid,
                                     u_int16_t max_bss_chan,
                                     u_int32_t max_dwell_time)
{
    struct ieee80211_resmgr_sm_event event;
    event.vap = NULL;
    event.chan = chan;
    event.max_time = max_bss_chan;
    event.max_dwell_time = max_dwell_time;

    /* Post event to ResMgr SM */
    if (ieee80211_resmgr_sm_dispatch(resmgr, IEEE80211_RESMGR_EVENT_OFFCHAN_REQUEST, &event) == EOK) {
        return EBUSY;  /* processing req asynchronously */
    } else {
        return EOK;    /* caller can change channel */
    }
}

/*
 * BSS channel request. (Scan Module)
 */
int ieee80211_resmgr_request_bsschan(ieee80211_resmgr_t resmgr,
                                     u_int16_t reqid)
{
    struct ieee80211_resmgr_sm_event event;
    struct vap_iter_check_state_params params;
    struct ieee80211com *ic = resmgr->ic;

    event.vap = NULL;
    event.chan = NULL; 

    if (ieee80211_resmgr_active(ic)) {
        /* ResMgr SM active, check for active VAPs */
        OS_MEMZERO(&params, sizeof(struct vap_iter_check_state_params));
        wlan_iterate_vap_list(ic, ieee80211_vap_iter_check_state, &params);

        /* If there are no VAPs active, there is no BSS channel. Scanner can continueously scan all channels */
        if (!params.vaps_running && !params.vap_starting) {
            return EINVAL;
        }
    }
    
    /* Post event to ResMgr SM */
    if (ieee80211_resmgr_sm_dispatch(resmgr, IEEE80211_RESMGR_EVENT_BSSCHAN_REQUEST, &event) == EOK){
        return EBUSY;  /* processing req asynchronously */
    } else {
        return EOK;    /* caller can change channel */
    }
}

/*
 * Channel Switch Request. (VAP Module)
 */
int ieee80211_resmgr_request_chanswitch(ieee80211_resmgr_t resmgr,
                                        ieee80211_vap_t vap, 
                                        struct ieee80211_channel *chan,
                                        u_int16_t reqid)
{
    struct ieee80211_resmgr_sm_event event;
    event.vap = vap;
    event.chan = chan;

    /* Post event to ResMgr SM */
    if (ieee80211_resmgr_sm_dispatch(resmgr, IEEE80211_RESMGR_EVENT_CHAN_SWITCH_REQUEST, &event) == EOK) {
        return EBUSY;  /* processing req asynchronously */
    } else {
        return EOK;    /* caller can change channel */
    }
}

/**************************************
 * Resouce Manager Creation/Deletion 
 ***************************************/

/*
 * Create a Resource manager.
 */
ieee80211_resmgr_t ieee80211_resmgr_create(struct ieee80211com *ic, ieee80211_resmgr_mode mode)
{
    ieee80211_resmgr_t    resmgr;

    if (ic->ic_tsf_timer == NULL) {
        printk("%s: Multi-Channel Scheduler unavailable since no TSF Timer.\n", __func__);
    }
    else {
        ic->ic_caps_ext |= IEEE80211_CEXT_MULTICHAN;
    }

    if (ic->ic_resmgr) {
        printk("%s : ResMgr already exists \n", __func__); 
        return NULL;
    }

    /* Allocate ResMgr data structures */
    resmgr = (ieee80211_resmgr_t) OS_MALLOC(ic->ic_osdev, 
                                            sizeof(struct ieee80211_resmgr),
                                            0);
    if (resmgr == NULL) {
        printk("%s : ResMgr memory alloction failed\n", __func__); 
        return NULL;
    }

    OS_MEMZERO(resmgr, sizeof(struct ieee80211_resmgr));
    resmgr->ic = ic;
    resmgr->mode = mode;

    /* Initialize resource manager state machine */
    if (ieee80211_resmgr_sm_create(resmgr) != EOK) {
        OS_FREE(resmgr);
        printk("%s : ieee80211_resmgr_sm_create failed \n", __func__);
        return NULL;
    }

    if (ic->ic_caps_ext & IEEE80211_CEXT_MULTICHAN) {
        if (ieee80211_resmgr_oc_scheduler_create(resmgr, OFF_CHAN_SCHED_POLICY_ROUND_ROBIN) != EOK) {
            OS_FREE(resmgr);
            printk("%s : ieee80211_resmgr_oc_scheduler_create failed \n", __func__);
            return NULL;
        }
        ASSERT(ic->ic_tsf_timer);
    }
    else {
        /* Do not support P2P */
        printk("Error: %s: no Multi-chan hw caps. Disabling Off Channel scheduler.\n", __func__);
    }

    IEEE80211_RESMGR_LOCK_INIT(resmgr);
    spin_lock_init(&resmgr->rm_handler_lock);
    OS_INIT_TIMER(ic->ic_osdev, &(resmgr->scandata.canpause_timer), ieee80211_resmgr_canpause_timeout_callback, resmgr);
    resmgr->pm_state= IEEE80211_PWRSAVE_FULL_SLEEP;
    return resmgr;
}

/*
 * This function called after all IC objects are already initialized.
 * Resource Manager and Scanner need this so they can register callback
 * functions with each other.
 */
void ieee80211_resmgr_create_complete(ieee80211_resmgr_t resmgr)
{
    int    rc;

    /* Register event handler with SCAN module */
    rc = ieee80211_scan_register_event_handler(resmgr->ic->ic_scanner, ieee80211_resmgr_scan_evhandler, resmgr);
    ASSERT(rc == EOK);
    if (rc != EOK) {
        printk("%s : ieee80211_scan_register_event_handler() failed rc=%08X\n", 
            __func__, rc);
    }
}

/*
 * Delete Resource manager.
 */
void ieee80211_resmgr_delete(ieee80211_resmgr_t resmgr)
{
    struct ieee80211com    *ic;
    
    if (!resmgr)
        return ;

    ic = resmgr->ic;

    /* No vaps should exist at this time */
    ASSERT(TAILQ_FIRST(&ic->ic_vaps) == NULL);

    OS_CANCEL_TIMER(&(resmgr->scandata.canpause_timer));
    OS_FREE_TIMER(&(resmgr->scandata.canpause_timer));
    IEEE80211_RESMGR_LOCK_DESTROY(resmgr);

    /* Stop and delete the Off-Channel Scheduler */
    if (ic->ic_caps_ext & IEEE80211_CEXT_MULTICHAN) {
        ieee80211_resmgr_oc_scheduler_delete(resmgr);
    }
    
    /* Stop and delete state machine */
    ieee80211_resmgr_sm_delete(resmgr);

    /* Free ResMgr data structures */
    OS_FREE(resmgr);
    ic->ic_resmgr = NULL;
}

/*
 * This function called before IC objects are deleted.
 * Resource Manager and Scanner need this so they can unregister callback
 * functions from each other.
 */
void ieee80211_resmgr_delete_prepare(ieee80211_resmgr_t resmgr)
{
    int    rc;

    /* Unregister event handler with SCAN module */
    rc = ieee80211_scan_unregister_event_handler(resmgr->ic->ic_scanner, ieee80211_resmgr_scan_evhandler, resmgr);
    if (rc != EOK) {
        printk("%s: wlan_scan_unregister_event_handler() failed handler=%p,%p rc=0x%x\n",
            __func__, ieee80211_resmgr_scan_evhandler, resmgr, rc);
        ASSERT(0);
    }
}

/*
 * Strings for the various resource manager notifications
 */
const char *ieee80211_resmgr_get_notification_type_name(ieee80211_resmgr_notification_type type)
{
    static const char*    notification_type_name[] = {
        /* IEEE80211_RESMGR_BSSCHAN_SWITCH_COMPLETE */ "BSSCHAN_SWITCH_COMPLETE",
        /* IEEE80211_RESMGR_OFFCHAN_SWITCH_COMPLETE */ "OFFCHAN_SWITCH_COMPLETE",
        /* IEEE80211_RESMGR_OFFCHAN_ABORTED         */ "OFFCHAN_ABORTED",
        /* IEEE80211_RESMGR_VAP_START_COMPLETE,     */ "VAP_START_COMPLETE",
        /* IEEE80211_RESMGR_CHAN_SWITCH_COMPLETE    */ "CHAN_SWITCH_COMPLETE",
        /* IEEE80211_RESMGR_SCHEDULE_BSS_COMPLETE   */ "SCHEDULE_BSS_COMPLETE"
    };

    ASSERT(IEEE80211_RESMGR_NOTIFICATION_TYPE_COUNT == IEEE80211_N(notification_type_name));
    ASSERT(type < IEEE80211_N(notification_type_name));

    return (notification_type_name[type]);
}

/*
 *  @Allocate a schedule request for submission to the Resource Manager off-channel scheduler. Upon success,
 *  a handle for the request is returned. Otherwise, NULL is returned to indicate a failure
 *  to allocate the request.
 *  ARGS:
 *      resmgr          :   handle to resource manager.
 *      vap             :   handle to the vap.
 *      start_tsf       :   Value of starting TSF for scheduling window.
 *      duration_msec   :   Air-time duration in milliseconds (msec).
 *      interval_msec   :   Air-time period or interval in milliseconds (msec).
 *      priority        :   Off-Channel scheduler priority, i.e. High or Low.
 *      chan            :   Channel information
 *      periodic        :   Is request a one-shot or reoccurring type request
 */
ieee80211_resmgr_oc_sched_req_handle_t ieee80211_resmgr_off_chan_sched_req_alloc(ieee80211_resmgr_t resmgr,
    ieee80211_vap_t vap, u_int32_t duration_msec, u_int32_t interval_msec,
    ieee80211_resmgr_oc_sched_prio_t priority, bool periodic)
{
    ieee80211_resmgr_oc_sched_req_handle_t req;
    
    req = ieee80211_resmgr_oc_sched_req_alloc(resmgr, vap, duration_msec, 
                interval_msec, priority, periodic, OC_SCHEDULER_SCAN_REQTYPE);
    
    return(req);
}

/*
 *  @Start or initiate the scheduling of air time for this schedule request handle.
 *  ARGS:
 *      resmgr          :   handle to resource manager
 *      req_handle      :   handle to off-channel scheduler request
 */
int ieee80211_resmgr_off_chan_sched_req_start(ieee80211_resmgr_t resmgr, 
        ieee80211_resmgr_oc_sched_req_handle_t req_handle)
{
    struct ieee80211_resmgr_sm_event event;
    int retval;

    retval = ieee80211_resmgr_oc_sched_req_start(resmgr, req_handle);

    OS_MEMZERO(&event, sizeof(struct ieee80211_resmgr_sm_event));

    /* Post event to ResMgr SM */
    if (ieee80211_resmgr_sm_dispatch(resmgr, 
                IEEE80211_RESMGR_EVENT_OFF_CHAN_SCHED_START_REQUEST, &event) == EOK) {
        return EBUSY;  /* processing req asynchronously */
    } else {
        return EOK;    /* caller can change channel */
    }
}

/*
 *  @Stop or cease the scheduling of air time for this schedule request handle.
 *  ARGS:
 *      resmgr          :   handle to resource manager.
 *      req_handle      :   handle to off-channel scheduler request.
 */

int ieee80211_resmgr_off_chan_sched_req_stop(ieee80211_resmgr_t resmgr,
        ieee80211_resmgr_oc_sched_req_handle_t req_handle)
{
    struct ieee80211_resmgr_sm_event event;
    int retval;

    retval = ieee80211_resmgr_oc_sched_req_stop(resmgr, req_handle);
    
    OS_MEMZERO(&event, sizeof(struct ieee80211_resmgr_sm_event));

    /* Post event to ResMgr SM */
    if (ieee80211_resmgr_sm_dispatch(resmgr, 
                IEEE80211_RESMGR_EVENT_OFF_CHAN_SCHED_STOP_REQUEST, (void *)&event) == EOK) {
        return EBUSY;  /* processing req asynchronously */
    } else {
        return EOK;    /* caller can change channel */
    }
}

/*
 *  @Free a previously allocated schedule request handle.
 *  ARGS:
 *      resmgr          :   handle to resource manager.
 *      req_handle      :   handle to off-channel scheduler request.
 */
void ieee80211_resmgr_off_chan_sched_req_free(ieee80211_resmgr_t resmgr,
    ieee80211_resmgr_oc_sched_req_handle_t req_handle)
{
    ieee80211_resmgr_oc_sched_req_free(resmgr, req_handle);
    return;
}

/*
 *  @Register a Notice Of Absence (NOA) handler
 *  ARGS:
 *      resmgr          :   handle to resource manager.
 *    vap           :    handle to VAP structure.
 *    handler           :       Function pointer for processing NOA schedules
 *      arg             :   Opaque parameter for handler
 *  
 * RETURNS:
 *  on success returns 0.
 *  on failure returns a negative value.
 * 
 */
int ieee80211_resmgr_register_noa_event_handler(ieee80211_resmgr_t resmgr,
    ieee80211_vap_t vap, ieee80211_resmgr_noa_event_handler handler, void *arg)
{
    return(ieee80211_resmgr_oc_sched_register_noa_event_handler(resmgr, vap, handler, arg));
}

/*
 *  @Unregister a Notice Of Absence (NOA) handler
 *  ARGS:
 *      resmgr          :   handle to resource manager.
 *    vap           :    handle to VAP structure.
 *  
 * RETURNS:
 *  on success returns 0.
 *  on failure returns a negative value.
 * 
 */
int ieee80211_resmgr_unregister_noa_event_handler(ieee80211_resmgr_t resmgr,
    ieee80211_vap_t vap)
{
    return(ieee80211_resmgr_oc_sched_unregister_noa_event_handler(resmgr, vap));
}

/*
 *  @Get the air-time limit when using off-channel scheduling
 *  ARGS:
 *      resmgr          :   handle to resource manager.
 *    vap           :    handle to VAP structure.
 *  
 * RETURNS:
 *   returns current air-time limit.
 * 
 */
u_int32_t ieee80211_resmgr_off_chan_sched_get_air_time_limit(ieee80211_resmgr_t resmgr,
    ieee80211_vap_t vap
    )
{
    ieee80211_resmgr_vap_priv_t resmgr_vap = ieee80211vap_get_resmgr(vap);

    return(ieee80211_resmgr_oc_sched_get_air_time_limit(resmgr, resmgr_vap));
}

/*
 *  @Set the air-time limit when using off-channel scheduling
 *  ARGS:
 *      resmgr          :   handle to resource manager.
 *    vap           :    handle to VAP structure.
 *      scheduled_air_time_limit    :   unsigned integer percentage with a range of 1 to 100
 *  
 * RETURNS:
 *  on success returns 0.
 *  on failure returns a negative value.
 * 
 */
int ieee80211_resmgr_off_chan_sched_set_air_time_limit(ieee80211_resmgr_t resmgr,
    ieee80211_vap_t vap,
    u_int32_t scheduled_air_time_limit
    )
{
    ieee80211_resmgr_vap_priv_t resmgr_vap = ieee80211vap_get_resmgr(vap);
    int retVal = EOK;

    if (ieee80211_resmgr_oc_sched_set_air_time_limit(resmgr, resmgr_vap, scheduled_air_time_limit) != EOK) {
        retVal = -1;
    }
    
    return(retVal);
}

#endif
