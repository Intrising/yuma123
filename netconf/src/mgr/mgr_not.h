#ifndef _H_mgr_not
#define _H_mgr_not
/*  FILE: mgr_not.h
*********************************************************************
*                                                                   *
*                         P U R P O S E                             *
*                                                                   *
*********************************************************************

    NETCONF protocol notification manager-side definitions

*********************************************************************
*                                                                   *
*                   C H A N G E         H I S T O R Y               *
*                                                                   *
*********************************************************************

date             init     comment
----------------------------------------------------------------------
03-jun-09    abb      Begun;
*/
#include <time.h>

#ifndef _H_cfg
#include "cfg.h"
#endif

#ifndef _H_rpc
#include "rpc.h"
#endif

#ifndef _H_ses
#include "ses.h"
#endif

#ifndef _H_status
#include "status.h"
#endif

#ifndef _H_tstamp
#include "tstamp.h"
#endif

#ifndef _H_xml_util
#include "xml_util.h"
#endif


/********************************************************************
*                                                                   *
*                         C O N S T A N T S                         *
*                                                                   *
*********************************************************************/


/********************************************************************
*                                                                   *
*                         T Y P E S                                 *
*                                                                   *
*********************************************************************/


/* struct to save and process an incoming notification */
typedef struct mgr_not_msg_t_ {
    dlq_hdr_t               qhdr;
    /* xml_msg_hdr_t           mhdr; */
    val_value_t            *notification;  /* parsed message */
    val_value_t            *eventTime;   /* ptr into notification */
    val_value_t            *eventType;   /* ptr into notification */
    status_t                res;         /* parse result */
} mgr_not_msg_t;


/* manager notification callback function
 *
 *  INPUTS:
 *   scb == session control block for session that got the reply
 *   msg == incoming notification msg
 */
typedef void (*mgr_not_cbfn_t) (ses_cb_t *scb,
				mgr_not_msg_t *msg);


/********************************************************************
*                                                                   *
*                        F U N C T I O N S                          *
*                                                                   *
*********************************************************************/

/* should call once to init module */
extern status_t 
    mgr_not_init (void);

/* should call once to cleanup module */
extern void 
    mgr_not_cleanup (void);

extern void
    mgr_not_free_msg (mgr_not_msg_t *msg);

extern void
    mgr_not_clean_msgQ (dlq_hdr_t *msgQ);


/* handle the <notification> element
 * called by mgr_top.c: 
 * This function is registered with top_register_node
 * for the module 'notification', top-node 'notification'
 */
extern void
    mgr_not_dispatch (ses_cb_t  *scb,
		      xml_node_t *top);

/* temp: just one notification callback; 
 * not per eventType or any other filter
 */
extern void
    mgr_not_set_callback_fn (mgr_not_cbfn_t cbfn);


#endif            /* _H_mgr_not */
