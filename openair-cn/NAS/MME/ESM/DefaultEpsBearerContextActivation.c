/*******************************************************************************
    OpenAirInterface
    Copyright(c) 1999 - 2014 Eurecom

    OpenAirInterface is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.


    OpenAirInterface is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with OpenAirInterface.The full GNU General Public License is
   included in this distribution in the file called "COPYING". If not,
   see <http://www.gnu.org/licenses/>.

  Contact Information
  OpenAirInterface Admin: openair_admin@eurecom.fr
  OpenAirInterface Tech : openair_tech@eurecom.fr
  OpenAirInterface Dev  : openair4g-devel@eurecom.fr

  Address      : Eurecom, Compus SophiaTech 450, route des chappes, 06451 Biot, France.

 *******************************************************************************/
/*****************************************************************************
Source      DefaultEpsBearerContextActivation.c

Version     0.1

Date        2013/01/28

Product     NAS stack

Subsystem   EPS Session Management

Author      Frederic Maurel

Description Defines the default EPS bearer context activation ESM
        procedure executed by the Non-Access Stratum.

        The purpose of the default bearer context activation procedure
        is to establish a default EPS bearer context between the UE
        and the EPC.

        The procedure is initiated by the network as a response to
        the PDN CONNECTIVITY REQUEST message received from the UE.
        It can be part of the attach procedure.

*****************************************************************************/

#include "esm_proc.h"
#include "commonDef.h"
#include "nas_log.h"

#include "esm_cause.h"
#include "esm_ebr.h"
#include "esm_ebr_context.h"

#include "emm_sap.h"

/****************************************************************************/
/****************  E X T E R N A L    D E F I N I T I O N S  ****************/
/****************************************************************************/

/****************************************************************************/
/*******************  L O C A L    D E F I N I T I O N S  *******************/
/****************************************************************************/


/*
 * --------------------------------------------------------------------------
 * Internal data handled by the default EPS bearer context activation
 * procedure in the MME
 * --------------------------------------------------------------------------
 */
/*
 * Timer handlers
 */
static void *_default_eps_bearer_activate_t3485_handler(void *);

/* Maximum value of the activate default EPS bearer context request
 * retransmission counter */
#define DEFAULT_EPS_BEARER_ACTIVATE_COUNTER_MAX 5

static int _default_eps_bearer_activate(emm_data_context_t *ctx, int ebi,
                                        const OctetString *msg);

/****************************************************************************/
/******************  E X P O R T E D    F U N C T I O N S  ******************/
/****************************************************************************/

/*
 * --------------------------------------------------------------------------
 *    Default EPS bearer context activation procedure executed by the MME
 * --------------------------------------------------------------------------
 */
/****************************************************************************
 **                                                                        **
 ** Name:    esm_proc_default_eps_bearer_context()                     **
 **                                                                        **
 ** Description: Allocates resources required for activation of a default  **
 **      EPS bearer context.                                       **
 **                                                                        **
 ** Inputs:  ueid:      UE local identifier                        **
 **          pid:       PDN connection identifier                  **
 **      qos:       EPS bearer level QoS parameters            **
 **      Others:    None                                       **
 **                                                                        **
 ** Outputs:     ebi:       EPS bearer identity assigned to the de-    **
 **             fault EPS bearer context                   **
 **      esm_cause: Cause code returned upon ESM procedure     **
 **             failure                                    **
 **      Return:    RETURNok, RETURNerror                      **
 **      Others:    None                                       **
 **                                                                        **
 ***************************************************************************/
int esm_proc_default_eps_bearer_context(emm_data_context_t *ctx, int pid,
                                        unsigned int *ebi,
                                        const esm_proc_qos_t *qos,
                                        int *esm_cause)
{
  LOG_FUNC_IN;

  LOG_TRACE(INFO, "ESM-PROC  - Default EPS bearer context activation "
            "(ueid="NAS_UE_ID_FMT", pid=%d, GBR UL %u GBR DL %u, MBR UL %u MBR DL %u QCI %u)",
            ctx->ueid,
            pid,
            qos->gbrUL,
            qos->gbrDL,
            qos->mbrUL,
            qos->mbrDL,
            qos->qci);

  /* Assign new EPS bearer context */
  *ebi = esm_ebr_assign(ctx, ESM_EBI_UNASSIGNED);

  if (*ebi != ESM_EBI_UNASSIGNED) {
    /* Create default EPS bearer context */
    *ebi = esm_ebr_context_create(ctx, pid, *ebi, TRUE, qos, NULL);

    if (*ebi == ESM_EBI_UNASSIGNED) {
      /* No resource available */
      LOG_TRACE(WARNING, "ESM-PROC  - Failed to create new default EPS "
                "bearer context (ebi=%d)", *ebi);
      *esm_cause = ESM_CAUSE_INSUFFICIENT_RESOURCES;
      LOG_FUNC_RETURN (RETURNerror);
    }

    LOG_FUNC_RETURN (RETURNok);
  }

  LOG_TRACE(WARNING, "ESM-PROC  - Failed to assign new EPS bearer context");
  *esm_cause = ESM_CAUSE_INSUFFICIENT_RESOURCES;
  LOG_FUNC_RETURN (RETURNerror);
}

/****************************************************************************
 **                                                                        **
 ** Name:    esm_proc_default_eps_bearer_context_request()             **
 **                                                                        **
 ** Description: Initiates the default EPS bearer context activation pro-  **
 **      cedure                                                    **
 **                                                                        **
 **      3GPP TS 24.301, section 6.4.1.2                           **
 **      The MME initiates the default EPS bearer context activa-  **
 **      tion procedure by sending an ACTIVATE DEFAULT EPS BEARER  **
 **      CONTEXT REQUEST message, starting timer T3485 and ente-   **
 **      ring state BEARER CONTEXT ACTIVE PENDING.                 **
 **                                                                        **
 ** Inputs:  is_standalone: Indicate whether the default bearer is     **
 **             activated as part of the attach procedure  **
 **             or as the response to a stand-alone PDN    **
 **             CONNECTIVITY REQUEST message               **
 **      ueid:      UE lower layer identifier                  **
 **      ebi:       EPS bearer identity                        **
 **      msg:       Encoded ESM message to be sent             **
 **      ue_triggered:  TRUE if the EPS bearer context procedure   **
 **             was triggered by the UE                    **
 **      Others:    None                                       **
 **                                                                        **
 ** Outputs:     None                                                      **
 **      Return:    RETURNok, RETURNerror                      **
 **      Others:    None                                       **
 **                                                                        **
 ***************************************************************************/
int esm_proc_default_eps_bearer_context_request(int is_standalone,
    emm_data_context_t *ctx, int ebi,
    OctetString *msg, int ue_triggered)
{
  int rc = RETURNok;

  LOG_FUNC_IN;


  if (is_standalone) {
    /* Send activate default EPS bearer context request message and
     * start timer T3485 */
    LOG_TRACE(INFO,"ESM-PROC  - Initiate standalone default EPS bearer context activation "
              "(ueid="NAS_UE_ID_FMT", ebi=%d)", ctx->ueid, ebi);
    rc = _default_eps_bearer_activate(ctx, ebi, msg);
  } else {
    LOG_TRACE(INFO,"ESM-PROC  - Initiate non standalone default EPS bearer context activation "
              "(ueid="NAS_UE_ID_FMT", ebi=%d)", ctx->ueid, ebi);
  }

  if (rc != RETURNerror) {
    /* Set the EPS bearer context state to ACTIVE PENDING */
    rc = esm_ebr_set_status(ctx, ebi, ESM_EBR_ACTIVE_PENDING, ue_triggered);

    if (rc != RETURNok) {
      /* The EPS bearer context was already in ACTIVE PENDING state */
      LOG_TRACE(WARNING, "ESM-PROC  - EBI %d was already ACTIVE PENDING",
                ebi);
    }
  }

  LOG_FUNC_RETURN (rc);
}

/****************************************************************************
 **                                                                        **
 ** Name:    esm_proc_default_eps_bearer_context_accept()              **
 **                                                                        **
 ** Description: Performs default EPS bearer context activation procedure  **
 **      accepted by the UE.                                       **
 **                                                                        **
 **      3GPP TS 24.301, section 6.4.1.3                           **
 **      Upon receipt of the ACTIVATE DEFAULT EPS BEARER CONTEXT   **
 **      ACCEPT message, the MME shall enter the state BEARER CON- **
 **      TEXT ACTIVE and stop the timer T3485, if it is running.   **
 **                                                                        **
 ** Inputs:  ueid:      UE local identifier                        **
 **      ebi:       EPS bearer identity                        **
 **      Others:    None                                       **
 **                                                                        **
 ** Outputs:     esm_cause: Cause code returned upon ESM procedure     **
 **             failure                                    **
 **      Return:    RETURNok, RETURNerror                      **
 **      Others:    None                                       **
 **                                                                        **
 ***************************************************************************/
int esm_proc_default_eps_bearer_context_accept(emm_data_context_t *ctx, int ebi,
    int *esm_cause)
{
  LOG_FUNC_IN;

  int rc;

  LOG_TRACE(INFO, "ESM-PROC  - Default EPS bearer context activation "
            "accepted by the UE (ueid="NAS_UE_ID_FMT", ebi=%d)", ctx->ueid, ebi);

  /* Stop T3485 timer if running */
  rc = esm_ebr_stop_timer(ctx, ebi);

  if (rc != RETURNerror) {
    /* Set the EPS bearer context state to ACTIVE */
    rc = esm_ebr_set_status(ctx, ebi, ESM_EBR_ACTIVE, FALSE);

    if (rc != RETURNok) {
      /* The EPS bearer context was already in ACTIVE state */
      LOG_TRACE(WARNING, "ESM-PROC  - EBI %d was already ACTIVE", ebi);
      *esm_cause = ESM_CAUSE_PROTOCOL_ERROR;
    }
  }

  LOG_FUNC_RETURN (rc);
}

/****************************************************************************
 **                                                                        **
 ** Name:    esm_proc_default_eps_bearer_context_reject()              **
 **                                                                        **
 ** Description: Performs default EPS bearer context activation procedure  **
 **      not accepted by the UE.                                   **
 **                                                                        **
 **      3GPP TS 24.301, section 6.4.1.4                           **
 **      Upon receipt of the ACTIVATE DEFAULT EPS BEARER CONTEXT   **
 **      REJECT message, the MME shall enter the state BEARER CON- **
 **      TEXT INACTIVE and stop the timer T3485, if it is running. **
 **                                                                        **
 ** Inputs:  ueid:      UE local identifier                        **
 **      ebi:       EPS bearer identity                        **
 **      Others:    None                                       **
 **                                                                        **
 ** Outputs:     esm_cause: Cause code returned upon ESM procedure     **
 **             failure                                    **
 **      Return:    RETURNok, RETURNerror                      **
 **      Others:    None                                       **
 **                                                                        **
 ***************************************************************************/
int esm_proc_default_eps_bearer_context_reject(emm_data_context_t *ctx, int ebi,
    int *esm_cause)
{
  LOG_FUNC_IN;

  int rc;

  LOG_TRACE(WARNING, "ESM-PROC  - Default EPS bearer context activation "
            "not accepted by the UE (ueid="NAS_UE_ID_FMT", ebi=%d)", ctx->ueid, ebi);

  /* Stop T3485 timer if running */
  rc = esm_ebr_stop_timer(ctx, ebi);

  if (rc != RETURNerror) {
    int pid, bid;
    /* Release the default EPS bearer context and enter state INACTIVE */
    rc = esm_proc_eps_bearer_context_deactivate(ctx, TRUE, ebi,
         &pid, &bid, NULL);

    if (rc != RETURNok) {
      /* Failed to release the default EPS bearer context */
      *esm_cause = ESM_CAUSE_PROTOCOL_ERROR;
    }
  }

  LOG_FUNC_RETURN (rc);
}

/****************************************************************************
 **                                                                        **
 ** Name:    esm_proc_default_eps_bearer_context_failure()             **
 **                                                                        **
 ** Description: Performs default EPS bearer context activation procedure  **
 **      upon receiving notification from the EPS Mobility Manage- **
 **      ment sublayer that EMM procedure that initiated EPS de-   **
 **      fault bearer context activation locally failed.           **
 **                                                                        **
 **      The MME releases the default EPS bearer context previous- **
 **      ly allocated when ACTIVATE DEFAULT EPS BEARER CONTEXT RE- **
 **      QUEST message was received.                               **
 **                                                                        **
 ** Inputs:  ueid:      UE local identifier                        **
 **      Others:    None                                       **
 **                                                                        **
 ** Outputs:     None                                                      **
 **      Return:    The identifier of the PDN connection the   **
 **             default EPS bearer context belongs to if   **
 **             successfully released;                     **
 **             RETURNerror  otherwise.                    **
 **      Others:    None                                       **
 **                                                                        **
 ***************************************************************************/
int esm_proc_default_eps_bearer_context_failure(emm_data_context_t *ctx)
{
  int rc = RETURNerror;
  int pid;

  LOG_FUNC_IN;

  LOG_TRACE(WARNING, "ESM-PROC  - Default EPS bearer context activation "
            "failure (ueid="NAS_UE_ID_FMT")", ctx->ueid);

  /* Get the EPS bearer identity of the EPS bearer context which is still
   * pending in the active pending state */
  int ebi = esm_ebr_get_pending_ebi(ctx, ESM_EBR_ACTIVE_PENDING);

  if (ebi != ESM_EBI_UNASSIGNED) {
    int bid;
    /* Release the default EPS bearer context and enter state INACTIVE */
    rc = esm_proc_eps_bearer_context_deactivate(ctx, TRUE, ebi,
         &pid, &bid, NULL);
  }

  if (rc != RETURNerror) {
    LOG_FUNC_RETURN (pid);
  }

  LOG_FUNC_RETURN (RETURNerror);
}



/****************************************************************************/
/*********************  L O C A L    F U N C T I O N S  *********************/
/****************************************************************************/

/*
 * --------------------------------------------------------------------------
 *              Timer handlers
 * --------------------------------------------------------------------------
 */
/****************************************************************************
 **                                                                        **
 ** Name:    _default_eps_bearer_activate_t3485_handler()              **
 **                                                                        **
 ** Description: T3485 timeout handler                                     **
 **                                                                        **
 **              3GPP TS 24.301, section 6.4.1.6, case a                   **
 **      On the first expiry of the timer T3485, the MME shall re- **
 **      send the ACTIVATE DEFAULT EPS BEARER CONTEXT REQUEST and  **
 **      shall reset and restart timer T3485. This retransmission  **
 **      is repeated four times, i.e. on the fifth expiry of timer **
 **      T3485, the MME shall release possibly allocated resources **
 **      for this activation and shall abort the procedure.        **
 **                                                                        **
 ** Inputs:  args:      handler parameters                         **
 **      Others:    None                                       **
 **                                                                        **
 ** Outputs:     None                                                      **
 **      Return:    None                                       **
 **      Others:    None                                       **
 **                                                                        **
 ***************************************************************************/
static void *_default_eps_bearer_activate_t3485_handler(void *args)
{
  LOG_FUNC_IN;

  int rc;

  /* Get retransmission timer parameters data */
  esm_ebr_timer_data_t *data = (esm_ebr_timer_data_t *)(args);

  /* Increment the retransmission counter */
  data->count += 1;

  LOG_TRACE(WARNING, "ESM-PROC  - T3485 timer expired (ueid="NAS_UE_ID_FMT", ebi=%d), "
            "retransmission counter = %d",
            data->ueid, data->ebi, data->count);

  if (data->count < DEFAULT_EPS_BEARER_ACTIVATE_COUNTER_MAX) {
    /* Re-send activate default EPS bearer context request message
     * to the UE */
    rc = _default_eps_bearer_activate(data->ctx, data->ebi, &data->msg);
  } else {
    /*
     * The maximum number of activate default EPS bearer context request
     * message retransmission has exceed
     */
    int pid, bid;
    /* Release the default EPS bearer context and enter state INACTIVE */
    rc = esm_proc_eps_bearer_context_deactivate(data->ctx, TRUE,
         data->ebi, &pid, &bid,
         NULL);

    if (rc != RETURNerror) {
      /* Stop timer T3485 */
      rc = esm_ebr_stop_timer(data->ctx, data->ebi);
    }
  }

  LOG_FUNC_RETURN (NULL);
}

/*
 * --------------------------------------------------------------------------
 *              MME specific local functions
 * --------------------------------------------------------------------------
 */

/****************************************************************************
 **                                                                        **
 ** Name:    _default_eps_bearer_activate()                            **
 **                                                                        **
 ** Description: Sends ACTIVATE DEFAULT EPS BEREAR CONTEXT REQUEST message **
 **      and starts timer T3485                                    **
 **                                                                        **
 ** Inputs:  ueid:      UE local identifier                        **
 **      ebi:       EPS bearer identity                        **
 **      msg:       Encoded ESM message to be sent             **
 **      Others:    None                                       **
 **                                                                        **
 ** Outputs:     None                                                      **
 **      Return:    RETURNok, RETURNerror                      **
 **      Others:    T3485                                      **
 **                                                                        **
 ***************************************************************************/
static int _default_eps_bearer_activate(emm_data_context_t *ctx, int ebi,
                                        const OctetString *msg)
{
  LOG_FUNC_IN;

  emm_sap_t emm_sap;
  int rc;

  /*
   * Notify EMM that an activate default EPS bearer context request message
   * has to be sent to the UE
   */
  emm_esm_data_t *emm_esm = &emm_sap.u.emm_esm.u.data;
  emm_sap.primitive = EMMESM_UNITDATA_REQ;
  emm_sap.u.emm_esm.ueid = ctx->ueid;
  emm_sap.u.emm_esm.ctx  = ctx;
  emm_esm->msg = *msg;
  rc = emm_sap_send(&emm_sap);

  if (rc != RETURNerror) {
    /* Start T3485 retransmission timer */
    rc = esm_ebr_start_timer(ctx, ebi, msg, T3485_DEFAULT_VALUE,
                             _default_eps_bearer_activate_t3485_handler);
  }

  LOG_FUNC_RETURN (rc);
}
