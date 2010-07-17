/*
 * Copyright (c) 2009, 2010, Andy Bierman
 * 
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.    
 */
#ifndef _H_agt_val
#define _H_agt_val

/*  FILE: agt_val.h
*********************************************************************
*								    *
*			 P U R P O S E				    *
*								    *
*********************************************************************

    NETCONF Server database callback handler

*********************************************************************
*								    *
*		   C H A N G E	 H I S T O R Y			    *
*								    *
*********************************************************************

date	     init     comment
----------------------------------------------------------------------
20-may-06    abb      Begun
30-sep-08    abb      Implement AGT_CB_TEST_APPLY and 
                      agt_val_split_root_test for YANG support of 
                      dummy running config commit-time validation
*/

#ifndef _H_agt
#include "agt.h"
#endif

#ifndef _H_cfg
#include "cfg.h"
#endif

#ifndef _H_ncxtypes
#include "ncxtypes.h.h"
#endif

#ifndef _H_op
#include "op.h"
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

#ifndef _H_val
#include "val.h"
#endif

#ifndef _H_xml_msg
#include "xml_msg.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/********************************************************************
*								    *
*			 C O N S T A N T S			    *
*								    *
*********************************************************************/

/********************************************************************
*								    *
*			     T Y P E S				    *
*								    *
*********************************************************************/

/********************************************************************
*								    *
*			F U N C T I O N S			    *
*								    *
*********************************************************************/

/* General val_value_t processing
 *
 * step 1: call agt_val_parse_nc
 * step 2: call val_add_defaults
 * step 3: call agt_val_rpc_xpath_check
 * step 4: call agt_val_instance_check
 *
 * Additional steps to write to a config database
 *
 * step 4: call agt_val_validate_write
 * step 5: call agt_val_apply_write
 *
 * Steps to test for commit-ready
 * 
 * step 6a: agt_val_root_check (candidate)
 * step 6b: agt_val_split_root_check (running)
 * step 7: agt_val_apply_commit
 */


/********************************************************************
* FUNCTION agt_val_rpc_xpath_check
* 
* Check for any nodes which are present
* but have false when-stmts associated
* with the node.  These are errors and
* need to be flagged as unknown-element
* 
* Any false nodes will be removed from the input PDU
* and discarded, after the error is recorded.
* This prevents false positives or negatives in
* the agt_val_instance_check, called after this function
*
* Also checks any false must-stmts for nodes
* which are present (after false when removal)
* These are flagged as 'must-violation' errors
* as per YANG, 13.4
*
* INPUTS:
*   scb == session control block (may be NULL; no session stats)
*   msg == xml_msg_hdr t from msg in progress 
*       == NULL MEANS NO RPC-ERRORS ARE RECORDED
*   rpcinput == RPC input node conceptually under rpcroot
*               except this rpcinput has no parent node
*               so a fake one will be termporarily added 
*               to prevent false XPath validation errors
*   rpcroot == RPC method node. 
*              The conceptual parent of this node 
*              is used as the document root (/rpc == /)
*
* OUTPUTS:
*   if false nodes found under :
*     they are deleted
*   if msg not NULL:
*      msg->msg_errQ may have rpc_err_rec_t structs added to it 
*      which must be freed by the called with the 
*      rpc_err_free_record function
*
* RETURNS:
*   status of the operation
*   NO_ERR if no false when or must statements found
*********************************************************************/
extern status_t 
    agt_val_rpc_xpath_check (ses_cb_t *scb,
			     xml_msg_hdr_t *msg,
			     val_value_t *rpcinput,
			     obj_template_t *rpcroot);


/********************************************************************
* FUNCTION agt_val_instance_check
* 
* Check for the proper number of object instances for
* the specified value struct.
* 
* The top-level value set passed cannot represent a choice
* or a case within a choice.
*
* This function is intended for validating PDUs (RPC requests)
* during the PDU processing.  It does not check the instance
* count or must-stmt expressions for any <config> (ncx:root)
* container.  This must be dome with the agt_val_root_check function.
*
* INPUTS:
*   scb == session control block (may be NULL; no session stats)
*   msg == xml_msg_hdr t from msg in progress 
*       == NULL MEANS NO RPC-ERRORS ARE RECORDED
*   valset == val_value_t list, leaf-list, or container to check
*   root == database root of 'valset'
*   layer == NCX layer calling this function (for error purposes only)
*
* OUTPUTS:
*   if msg not NULL:
*      msg->msg_errQ may have rpc_err_rec_t structs added to it 
*      which must be freed by the called with the 
*      rpc_err_free_record function
*
* RETURNS:
*   status of the operation, NO_ERR if no validation errors found
*********************************************************************/
extern status_t 
    agt_val_instance_check (ses_cb_t *scb,
			    xml_msg_hdr_t *msg,
			    val_value_t *valset,
			    val_value_t *root,
			    ncx_layer_t layer);


/********************************************************************
* FUNCTION agt_val_root_check
* 
* Check for the proper number of object instances for
* the specified configuration database
* 
* INPUTS:
*   scb == session control block (may be NULL; no session stats)
*   msg == RPC msg in progress 
*       == NULL MEANS NO RPC-ERRORS ARE RECORDED
*   root == val_value_t for the target config being checked
*
* OUTPUTS:
*   if msg not NULL:
*      msg->mhdr.msg_errQ may have rpc_err_rec_t 
*      structs added to it which must be freed by the 
*      caller with the rpc_err_free_record function
*
* RETURNS:
*   status of the operation, NO_ERR if no validation errors found
*********************************************************************/
extern status_t 
    agt_val_root_check (ses_cb_t *scb,
			rpc_msg_t *msg,
			val_value_t *root);


/********************************************************************
* FUNCTION agt_val_split_root_check
* 
* Check for the proper number of object instances for
* the specified configuration database.  Conceptually
* combine the newroot and root and check that.
* 
* This function is only used if the cfg target is RUNNING
* The CANDIDATE cfg should use the agt_val_root_check
* instead for a pre-commit test
*
* INPUTS:
*   scb == session control block (may be NULL; no session stats)
*   msg == RPC message in progress 
*       == NULL MEANS NO RPC-ERRORS ARE RECORDED
*   newroot == val_value_t for the edit-config config contents
*   root == val_value_t for the target config being checked
*   defop == the starting default-operation value
*
* OUTPUTS:
*   if msg not NULL:
*      msg->msg_errQ may have rpc_err_rec_t structs added to it 
*      which must be freed by the called with the 
*      rpc_err_free_record function
*
* RETURNS:
*   status of the operation, NO_ERR if no validation errors found
*********************************************************************/
extern status_t 
    agt_val_split_root_check (ses_cb_t *scb,
			      rpc_msg_t *msg,
			      val_value_t *newroot,
			      val_value_t *root,
			      op_editop_t defop);


/********************************************************************
* FUNCTION agt_val_validate_write
* 
* Validate the requested <edit-config> write operation
*
* Check all the embedded operation attributes against
* the default-operation and maintained current operation.
*
* Invoke all the user AGT_CB_VALIDATE callbacks for a 
* 'new value' and 'existing value' pairs, for a given write operation, 
*
* These callbacks are invoked bottom-up, so the first step is to
* step through all the child nodes and traverse the
* 'new' data model (from the PDU) all the way to the leaf nodes
*
* The operation attribute is checked against the real data model
* on the way down the tree, and the user callbacks are invoked
* bottom-up on the way back.  This way, the user callbacks can
* share sub-tree validation routines, and perhaps add additional
* <rpc-error> information, based on the context and specific errors
* reported from 'below'.
*
* INPUTS:
*   scb == session control block
*   msg == incoming rpc_msg_t in progress
*   target == cfg_template_t for the config database to write 
*          == NULL for no actual write acess (validate only)
*   valroot == the val_value_t struct containing the root
*              (NCX_BT_CONTAINER, ncx:root)
*              datatype representing the config root with
*              proposed changes to the target
*   editop == requested start-state write operation
*             (usually from the default-operation parameter)
* OUTPUTS:
*   rpc_err_rec_t structs may be malloced and added to the msg->rpc_errQ
*
* RETURNS:
*   status of the operation
*********************************************************************/
extern status_t
    agt_val_validate_write (ses_cb_t  *scb,
			    rpc_msg_t  *msg,
			    cfg_template_t *target,
			    val_value_t *valroot,
			    op_editop_t  editop);




/********************************************************************
* FUNCTION agt_val_apply_write
* 
* Apply the requested write operation
*
* Invoke all the AGT_CB_APPLY callbacks for a 
* source and target and write operation
*
* INPUTS:
*   scb == session control block
*   msg == incoming rpc_msg_t in progress
*   target == cfg_template_t for the config database to write
*   pducfg == the 'root' value struct that represents the
*             tree of changes to apply to the target
*   editop == requested start-state write operation
*             (usually from the default-operation parameter)
*
* OUTPUTS:
*   rpc_err_rec_t structs may be malloced and added to the msg->mhsr.errQ
*
* RETURNS:
*   status
*********************************************************************/
extern status_t
    agt_val_apply_write (ses_cb_t  *scb,
			 rpc_msg_t  *msg,
			 cfg_template_t *target,
			 val_value_t    *pducfg,
			 op_editop_t  editop);


/********************************************************************
* FUNCTION agt_val_apply_commit
* 
* Apply the requested commit operation
*
* Invoke all the AGT_CB_COMMIT callbacks for a 
* source and target and write operation
*
* INPUTS:
*   scb == session control block
*   msg == incoming commit rpc_msg_t in progress
*   source == cfg_template_t for the source (candidate)
*   target == cfg_template_t for the config database to 
*             write (running)
*   save_nvstore == TRUE if the mirrored NV-store
*                   should be updated after the commit is done
*                   FALSE if this is the start of a confirmed-commit
*                   so the NV-store update is deferred
*                   Never save to NV-store if :startup is supported
*
* OUTPUTS:
*   rpc_err_rec_t structs may be malloced and added 
*   to the msg->mhsr.errQ
*
* RETURNS:
*   status
*********************************************************************/
extern status_t
    agt_val_apply_commit (ses_cb_t  *scb,
			  rpc_msg_t  *msg,
			  cfg_template_t *source,
			  cfg_template_t *target,
                          boolean save_nvstore);


/********************************************************************
* FUNCTION agt_val_check_commit_locks
* 
* Check if the requested commit operation
* would cause any partial lock violations 
* in the running config
* Invoke all the AGT_CB_COMMIT_CHECK callbacks for a 
* source and target and write operation
*
* INPUTS:
*   scb == session control block
*   msg == incoming commit rpc_msg_t in progress
*   source == cfg_template_t for the source (candidate)
*   target == cfg_template_t for the config database to 
*             write (running)
*
* OUTPUTS:
*   rpc_err_rec_t structs may be malloced and added 
*   to the msg->mhdr.errQ
*
* RETURNS:
*   status
*********************************************************************/
extern status_t
    agt_val_check_commit_locks (ses_cb_t  *scb,
                                rpc_msg_t  *msg,
                                cfg_template_t *source,
                                cfg_template_t *target);

#ifdef __cplusplus
}  /* end extern 'C' */
#endif

#endif	    /* _H_agt_val */
