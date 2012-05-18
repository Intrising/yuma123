
/* 

 * Copyright (c) 2008 - 2012, Andy Bierman, All Rights Reserved.
 * All Rights Reserved.
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 *
 * Instrumentation Written by Igor Smolyar

*** Generated by yangdump 1.15.1351

    module yuma-arp
    revision 2011-08-25

    namespace http://netconfcentral.org/ns/yuma-arp
    organization Netconf Central

 */

//#include <xmlstring.h>

#if !defined(CYGWIN) && !defined(MACOSX)
#define BUILD_ARP 1
#endif


#include <sys/ioctl.h>

#ifdef BUILD_ARP
#include <net/if_arp.h>
#include <net/if.h>
#include <string.h>
#include <arpa/inet.h>
#include <errno.h>
#include <sys/types.h>
#include <dirent.h>
#endif

#include "procdefs.h"
#include "agt.h"
#include "agt_cb.h"
#include "agt_timer.h"
#include "agt_util.h"
#include "agt_yuma_arp.h"
#include "dlq.h"
#include "ncx.h"
#include "ncxmod.h"
#include "ncxtypes.h"
#include "status.h"

#ifdef BUILD_ARP

/* module static variables */
static ncx_module_t *yuma_arp_mod;
static obj_template_t *arp_obj;
static val_value_t *arp_val;

/* put your static variables here */
static int counter;
static int proc_net_arp_ok;

static void write_file(const char *name, uint32 value) {
    FILE *fp;
    if ((fp = fopen(name, "w")) == NULL) {
	log_debug("\nCould not open the file: %s\n", name);
	return;
    }

    fprintf(fp, "%u", value);
    fclose(fp);
    return;
}

static int add_leaf (val_value_t * parent,
		    const xmlChar * valName,
		    xmlChar * valValue,
		    status_t *res)
{
    val_value_t * newVal;
    obj_template_t * newObj;

    newObj = obj_find_child(parent->obj,
	    y_yuma_arp_M_yuma_arp,
	    valName);
    if (newObj == NULL) {
	*res = SET_ERROR(ERR_NCX_DEF_NOT_FOUND);
	return -1;
    }

    newVal = val_make_simval_obj(newObj, valValue, res);
    if (newVal == NULL) {
	return -1;
    }

    val_add_child(newVal, parent);
    return 0;
}

/********************************************************************
 * FUNCTION make_arp_entry
 *
 * Make the starter arp entry for the specified name
 *
 * INPUTS:
 *   entryobj == object template to use
 *   nameptr == name string, zero-terminated
 *   res == address of return status
 *
 * OUTPUTS:
 *   *res == return status
 *
 * RETURNS:
 *    pointer to the new entry or NULL if malloc failed
 *********************************************************************/
static val_value_t *
make_arp_entry (val_value_t *dynamic_arp_val,
	xmlChar *ipptr,
	xmlChar *macptr,
	status_t *res)
{
    obj_template_t     *entryobj;
    obj_template_t     *dynamic_arp_obj;
    val_value_t        *entryval;

    *res = NO_ERR;

    if((ipptr == NULL) || (macptr == NULL)) {
	log_debug("\n IP or MAC can not be NULL!");
	*res = SET_ERROR(ERR_NCX_INVALID_VALUE);
	return NULL;
    }

    log_debug2("\nMake dynamic ARP entry ip:%s mac:%s", ipptr, macptr);

    dynamic_arp_obj = dynamic_arp_val->obj;
    entryobj = obj_find_child(dynamic_arp_obj,
	    y_yuma_arp_M_yuma_arp,
	    y_yuma_arp_N_dynamic_arp);
    if (entryobj == NULL) {
	*res = SET_ERROR(ERR_NCX_DEF_NOT_FOUND);
	return NULL;
    }

    entryval = val_new_value();
    if (entryval == NULL) {
	*res = ERR_INTERNAL_MEM;
	return NULL;
    }
    val_init_from_template(entryval, entryobj);

    /* pass off entryval memory here */
    val_add_child(entryval, dynamic_arp_val);


    if (add_leaf (entryval, y_yuma_arp_N_ip_address, ipptr, res) == -1)
    {
	return NULL;
    }

    *res = val_gen_index_chain(entryval->obj, entryval);

    if (*res != NO_ERR) {
	return NULL;
    }


    if (add_leaf (entryval, y_yuma_arp_N_mac_address, macptr, res) == -1)
    {
	return NULL;
    }

    return entryval;

} /* make_arp_entry */


/********************************************************************
 * FUNCTION parse_buffer
 * 
 * Parse single line from /proc/net/arp and fetch mac and ip addresses
 * 
 * INPUTS:
 *     currChar = beginning of line to parse
 *     ip_address = buffer to fill with ip address from line
 *     mac_address = buffer to fill with mac address from line
 * 
 * RETURNS:
 *     error status
 ********************************************************************/
static status_t parse_buffer( 
	xmlChar *currChar,
	xmlChar *ip_address,
	xmlChar *mac_address)
{ 
    status_t res;
    xmlChar *startIP, *endIP, *startMAC, *endMAC, *startFlag;
    int i;

    if((ip_address == NULL) || (mac_address == NULL)) {
	log_debug("\n IP or MAC can not be NULL!");
	return ERR_NCX_INVALID_VALUE;
    }

    res = NO_ERR;

    /* skip white spaces */
    while (*currChar && xml_isspace(*currChar)) {
	currChar++;
    }

    if (*currChar == '\0') {
	/* not expecting a line with just whitespace on it */
	return  ERR_NCX_SKIPPED;
    } else {
	startIP = currChar;
    }

    /* get the end of the interface name */
    while (*currChar && !xml_isspace(*currChar)) {
	currChar++;
	if (*currChar == '\0') {
	    /* not expecting a line with just foo on it */
	    return ERR_NCX_SKIPPED;
	}
    }

    endIP = currChar++;

    /* skipping three parts of line, in following order
     * 1 - spaces and HW type
     * 2 - spaces and Flags 
     * 3 - spaces before MAC 
     */
    for (i = 0; i < 3; i++)
    {
	while (*currChar && xml_isspace(*currChar)) {
	    currChar++;
	}	
	if (*currChar == '\0') {
	    return ERR_NCX_SKIPPED;
	}
	if (i == 2) {
	    /* MAC is found */
	    break;
	}
	startFlag = currChar;

	/* skipping non-spaces */
	while (*currChar && !xml_isspace(*currChar)) {
	    currChar++;
	    if (*currChar == '\0') {
		return ERR_NCX_SKIPPED;
	    }
	}
	if (i == 1) {
	    /* FLag is found - flag defines type of arp entry */
	    /* We interested only in dynamic entries */
	    if (xml_strncmp((const xmlChar *) startFlag, MAC_DYNAMIC, 
			currChar - startFlag) != 0) {
		/* if arp entry is static - no point to parse further */
		return ERR_NCX_SKIPPED;	
	    }
	}
    }

    startMAC = currChar;
    /* find the end of MAC str */
    while (*currChar && !xml_isspace(*currChar)) {
	currChar++;
	if (*currChar == '\0') {
	    return ERR_NCX_SKIPPED;
	}
    }

    endMAC = currChar;

    if( ip_address && ((endIP - startIP) < ADDRESS_SIZE)) {
	xml_strncpy (ip_address, startIP, endIP - startIP);
	ip_address [endIP - startIP] = '\0';
    } else {
	log_debug("\nLine parsing problem - ip address is to large\n");
    }

    if( mac_address && ((endMAC - startMAC) < ADDRESS_SIZE)) {
	xml_strncpy (mac_address, startMAC, endMAC - startMAC);
	mac_address [endMAC - startMAC] = '\0';
    } else {
	log_debug("\nLine parsing problem - mac address is too large\n");
    }

    return res;

} /* end of parse_buffer */


/********************************************************************
 * FUNCTION modify_arp
 * 
 * Modifies static arp entries
 * 
 * INPUTS:
 *     ipval = ip-address data node
 *     macval = mac-address data node
 *     action = action to perform
 *     res = store result of the action
 * 
 ********************************************************************/
static void modify_arp (
	val_value_t *ipval, 
	val_value_t *macval, 
	int action,
	status_t *res)
{
    struct arpreq req ;
    struct sockaddr_in sa_in;
    int sockfd = 0, i, tmp[HW_OCTETS];

    memset((char *) &req, 0, sizeof(req));
    memset(&sa_in, 0, sizeof(sa_in));

    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        log_debug("\nFailed to open a socket for ioctl.\n");
        *res = SET_ERROR(ERR_NCX_OPERATION_FAILED);
        return;
    }

    req.arp_pa.sa_family = AF_INET ;
    sa_in.sin_family = AF_INET;

    /* set ip */
    sa_in.sin_port = 0;
    if(inet_pton(AF_INET, 
		(const char *)(VAL_STR(ipval)) , &sa_in.sin_addr) != 1) {
        log_debug("\nINET_PTON error: %s\n", strerror(errno));
        *res = SET_ERROR(ERR_NCX_OPERATION_FAILED);
        return;
    }
    memcpy(&req.arp_pa , &sa_in, sizeof(sa_in));

    /* set hw address */
    if(sscanf((const char *)VAL_STR(macval), "%2x:%2x:%2x:%2x:%2x:%2x", 
          &tmp[0], &tmp[1], &tmp[2], &tmp[3], &tmp[4], &tmp[5]) != HW_OCTETS) {
        log_debug("\nWrong HW mac in data node: mac is %s\n", VAL_STR(macval));
        *res = SET_ERROR(ERR_NCX_OPERATION_FAILED);
        return;
    }

    for(i=0; i<HW_OCTETS;i++) {
        req.arp_ha.sa_data[i] = tmp[i];
    }

    req.arp_ha.sa_family = ARPHRD_ETHER;

    /* set permanent flag */
    req.arp_flags = ATF_PERM | ATF_COM;

    /* now issue the ioctl with requested action */
    switch(action){
    case(ARP_ADD):
        if(ioctl(sockfd, SIOCSARP,(caddr_t)&req) < 0) {
            log_debug("\nIOCTL error: %s\n", strerror(errno));
        }
        break;
    case(ARP_DEL):
        if(ioctl(sockfd, SIOCDARP,(caddr_t)&req) <0) {
            log_debug("\nIOCTL error: %s\n", strerror(errno));
        }
        break;
    default:
        log_debug("\nrong IOCTL command\n");
        *res = SET_ERROR(ERR_NCX_WRONG_VAL);
        break;
    }

    return;
}

/********************************************************************
* FUNCTION y_yuma_arp_arp_arp_settings_maximum_entries_edit
* 
* Edit database object callback
* Path: /arp/arp-settings/maximum-entries
* Add object instrumentation in COMMIT phase.
* 
* INPUTS:
*     see agt/agt_cb.h for details
* 
* RETURNS:
*     error status
********************************************************************/
static status_t
    y_yuma_arp_arp_arp_settings_maximum_entries_edit (
        ses_cb_t *scb,
        rpc_msg_t *msg,
        agt_cbtyp_t cbtyp,
        op_editop_t editop,
        val_value_t *newval,
        val_value_t *curval)
{
    status_t res;
    val_value_t *errorval;
    const xmlChar *errorstr;

    res = NO_ERR;
    errorval = NULL;
    errorstr = NULL;
    if (LOGDEBUG) {
        log_debug("\nEnter y_yuma_arp_arp_arp_settings_maximum_entries_edit "
                "callback for %s phase", agt_cbtype_name(cbtyp));
    }

    /* remove the next line if newval is used */
    (void)newval;

    /* remove the next line if curval is used */
    (void)curval;

    switch (cbtyp) {
    case AGT_CB_VALIDATE:
        /* description-stmt validation here */
        break;
    case AGT_CB_APPLY:
        /* database manipulation done here */
        break;
    case AGT_CB_COMMIT:
        /* device instrumentation done here */
	if(editop != OP_EDITOP_DELETE) {
	    uint32 val = VAL_UINT(newval);
	    write_file(TRESH1, val / 8);
	    write_file(TRESH2, val / 2);
	    write_file(TRESH3, val);
	}
        switch (editop) {
        case OP_EDITOP_LOAD:
            break;
        case OP_EDITOP_MERGE:
            break;
        case OP_EDITOP_REPLACE:
            break;
        case OP_EDITOP_CREATE:
            break;
        case OP_EDITOP_DELETE:
            break;
        default:
            res = SET_ERROR(ERR_INTERNAL_VAL);
        }
        break;
    case AGT_CB_ROLLBACK:
        /* undo device instrumentation here */
        break;
    default:
        res = SET_ERROR(ERR_INTERNAL_VAL);
    }

    /* if error: set the res, errorstr, and errorval parms */
    if (res != NO_ERR) {
        agt_record_error(
            scb,
            &msg->mhdr,
            NCX_LAYER_CONTENT,
            res,
            NULL,
            NCX_NT_STRING,
            errorstr,
            NCX_NT_VAL,
            errorval);
    }
    
    return res;

} /* y_yuma_arp_arp_arp_settings_maximum_entries_edit */


/********************************************************************
* FUNCTION y_yuma_arp_arp_arp_settings_validity_timeout_edit
* 
* Edit database object callback
* Path: /arp/arp-settings/validity-timeout
* Add object instrumentation in COMMIT phase.
* 
* INPUTS:
*     see agt/agt_cb.h for details
* 
* RETURNS:
*     error status
********************************************************************/
static status_t
    y_yuma_arp_arp_arp_settings_validity_timeout_edit (
        ses_cb_t *scb,
        rpc_msg_t *msg,
        agt_cbtyp_t cbtyp,
        op_editop_t editop,
        val_value_t *newval,
        val_value_t *curval)
{
    status_t res;
    val_value_t *errorval;
    const xmlChar *errorstr;

    res = NO_ERR;
    errorval = NULL;
    errorstr = NULL;
    if (LOGDEBUG) {
        log_debug("\nEnter y_yuma_arp_arp_arp_settings_validity_timeout_edit "
                "callback for %s phase", agt_cbtype_name(cbtyp));
    }

    /* remove the next line if newval is used */
    (void)newval;

    /* remove the next line if curval is used */
    (void)curval;

    switch (cbtyp) {
    case AGT_CB_VALIDATE:
        /* description-stmt validation here */
        break;
    case AGT_CB_APPLY:
        /* database manipulation done here */
        break;
    case AGT_CB_COMMIT:
        /* device instrumentation done here */
        if(editop != OP_EDITOP_DELETE) {
            DIR *dip;
            struct dirent *dit;
            char tmp[255];

            if ((dip = opendir(NEIGH_DIR)) == NULL) {
                log_debug("\nCan not open dir %s\n", NEIGH_DIR);
                return ERR_NCX_OPERATION_FAILED;
            }

            /* now for each interface update stale time */
            while ((dit = readdir(dip)) != NULL) {
                /* skip "." and ".." entries */ 
                if ((strncmp(dit->d_name, ".", strlen(dit->d_name)) == 0) || 
                    strncmp(dit->d_name, "..", strlen(dit->d_name)) == 0) {
                    continue;
                }

                snprintf(tmp, 255, "/proc/sys/net/ipv4/neigh/%s/gc_stale_time",
                         dit->d_name);
                write_file(tmp, VAL_UINT(newval));
            }

            closedir(dip);

        }

        switch (editop) {
        case OP_EDITOP_LOAD:
            break;
        case OP_EDITOP_MERGE:
            break;
        case OP_EDITOP_REPLACE:
            break;
        case OP_EDITOP_CREATE:
            break;
        case OP_EDITOP_DELETE:
            break;
        default:
            res = SET_ERROR(ERR_INTERNAL_VAL);
        }
        break;
    case AGT_CB_ROLLBACK:
        /* undo device instrumentation here */
        break;
    default:
        res = SET_ERROR(ERR_INTERNAL_VAL);
    }

    /* if error: set the res, errorstr, and errorval parms */
    if (res != NO_ERR) {
        agt_record_error(
            scb,
            &msg->mhdr,
            NCX_LAYER_CONTENT,
            res,
            NULL,
            NCX_NT_STRING,
            errorstr,
            NCX_NT_VAL,
            errorval);
    }
    
    return res;

} /* y_yuma_arp_arp_arp_settings_validity_timeout_edit */


/********************************************************************
* FUNCTION y_yuma_arp_arp_arp_settings_edit
* 
* Edit database object callback
* Path: /arp/arp-settings
* Add object instrumentation in COMMIT phase.
* 
* INPUTS:
*     see agt/agt_cb.h for details
* 
* RETURNS:
*     error status
********************************************************************/
static status_t
    y_yuma_arp_arp_arp_settings_edit (
        ses_cb_t *scb,
        rpc_msg_t *msg,
        agt_cbtyp_t cbtyp,
        op_editop_t editop,
        val_value_t *newval,
        val_value_t *curval)
{
    status_t res;
    val_value_t *errorval;
    const xmlChar *errorstr;

    res = NO_ERR;
    errorval = NULL;
    errorstr = NULL;
    if (LOGDEBUG) {
        log_debug("\nEnter y_yuma_arp_arp_arp_settings_edit callback "
                "for %s phase", agt_cbtype_name(cbtyp));
    }

    /* remove the next line if newval is used */
    (void)newval;

    /* remove the next line if curval is used */
    (void)curval;

    switch (cbtyp) {
    case AGT_CB_VALIDATE:
        /* description-stmt validation here */
        break;
    case AGT_CB_APPLY:
        /* database manipulation done here */
        break;
    case AGT_CB_COMMIT:
        /* device instrumentation done here */
        switch (editop) {
        case OP_EDITOP_LOAD:
            break;
        case OP_EDITOP_MERGE:
            break;
        case OP_EDITOP_REPLACE:
            break;
        case OP_EDITOP_CREATE:
            break;
        case OP_EDITOP_DELETE:
            break;
        default:
            res = SET_ERROR(ERR_INTERNAL_VAL);
        }
        break;
    case AGT_CB_ROLLBACK:
        /* undo device instrumentation here */
        break;
    default:
        res = SET_ERROR(ERR_INTERNAL_VAL);
    }

    /* if error: set the res, errorstr, and errorval parms */
    if (res != NO_ERR) {
        agt_record_error(
            scb,
            &msg->mhdr,
            NCX_LAYER_CONTENT,
            res,
            NULL,
            NCX_NT_STRING,
            errorstr,
            NCX_NT_VAL,
            errorval);
    }
    
    return res;

} /* y_yuma_arp_arp_arp_settings_edit */


/********************************************************************
* FUNCTION y_yuma_arp_arp_static_arps_static_arp_ip_address_edit
* 
* Edit database object callback
* Path: /arp/static-arps/static-arp/ip-address
* Add object instrumentation in COMMIT phase.
* 
* INPUTS:
*     see agt/agt_cb.h for details
* 
* RETURNS:
*     error status
********************************************************************/
static status_t
    y_yuma_arp_arp_static_arps_static_arp_ip_address_edit (
        ses_cb_t *scb,
        rpc_msg_t *msg,
        agt_cbtyp_t cbtyp,
        op_editop_t editop,
        val_value_t *newval,
        val_value_t *curval)
{
    status_t res;
    val_value_t *errorval;
    const xmlChar *errorstr;

    res = NO_ERR;
    errorval = NULL;
    errorstr = NULL;
    if (LOGDEBUG) {
        log_debug(
           "\nEnter y_yuma_arp_arp_static_arps_static_arp_ip_address_edit "
           "callback for %s phase", agt_cbtype_name(cbtyp));
    }

    /* remove the next line if newval is used */
    (void)newval;

    /* remove the next line if curval is used */
    (void)curval;

    switch (cbtyp) {
    case AGT_CB_VALIDATE:
        /* description-stmt validation here */
        break;
    case AGT_CB_APPLY:
        /* database manipulation done here */
        break;
    case AGT_CB_COMMIT:
        /* device instrumentation done here */
        switch (editop) {
        case OP_EDITOP_LOAD:
            break;
        case OP_EDITOP_MERGE:
            break;
        case OP_EDITOP_REPLACE:
            break;
        case OP_EDITOP_CREATE:
            break;
        case OP_EDITOP_DELETE:
            break;
        default:
            res = SET_ERROR(ERR_INTERNAL_VAL);
        }
        break;
    case AGT_CB_ROLLBACK:
        /* undo device instrumentation here */
        break;
    default:
        res = SET_ERROR(ERR_INTERNAL_VAL);
    }

    /* if error: set the res, errorstr, and errorval parms */
    if (res != NO_ERR) {
        agt_record_error(
            scb,
            &msg->mhdr,
            NCX_LAYER_CONTENT,
            res,
            NULL,
            NCX_NT_STRING,
            errorstr,
            NCX_NT_VAL,
            errorval);
    }
    
    return res;

} /* y_yuma_arp_arp_static_arps_static_arp_ip_address_edit */


/********************************************************************
* FUNCTION y_yuma_arp_arp_static_arps_static_arp_mac_address_edit
* 
* Edit database object callback
* Path: /arp/static-arps/static-arp/mac-address
* Add object instrumentation in COMMIT phase.
* 
* INPUTS:
*     see agt/agt_cb.h for details
* 
* RETURNS:
*     error status
********************************************************************/
static status_t
    y_yuma_arp_arp_static_arps_static_arp_mac_address_edit (
        ses_cb_t *scb,
        rpc_msg_t *msg,
        agt_cbtyp_t cbtyp,
        op_editop_t editop,
        val_value_t *newval,
        val_value_t *curval)
{
    status_t res;
    val_value_t *errorval;
    const xmlChar *errorstr;

    res = NO_ERR;
    errorval = NULL;
    errorstr = NULL;
    if (LOGDEBUG) {
        log_debug(
           "\nEnter y_yuma_arp_arp_static_arps_static_arp_mac_address_edit "
           "callback for %s phase", agt_cbtype_name(cbtyp));
    }

    /* remove the next line if newval is used */
    (void)newval;

    /* remove the next line if curval is used */
    (void)curval;

    switch (cbtyp) {
    case AGT_CB_VALIDATE:
        /* description-stmt validation here */
        break;
    case AGT_CB_APPLY:
        /* database manipulation done here */
        break;
    case AGT_CB_COMMIT:
        /* device instrumentation done here */
        switch (editop) {
        case OP_EDITOP_LOAD:
            break;
        case OP_EDITOP_MERGE:
            break;
        case OP_EDITOP_REPLACE:
            break;
        case OP_EDITOP_CREATE:
            break;
        case OP_EDITOP_DELETE:
            break;
        default:
            res = SET_ERROR(ERR_INTERNAL_VAL);
        }
        break;
    case AGT_CB_ROLLBACK:
        /* undo device instrumentation here */
        break;
    default:
        res = SET_ERROR(ERR_INTERNAL_VAL);
    }

    /* if error: set the res, errorstr, and errorval parms */
    if (res != NO_ERR) {
        agt_record_error(
            scb,
            &msg->mhdr,
            NCX_LAYER_CONTENT,
            res,
            NULL,
            NCX_NT_STRING,
            errorstr,
            NCX_NT_VAL,
            errorval);
    }
    
    return res;

} /* y_yuma_arp_arp_static_arps_static_arp_mac_address_edit */


/********************************************************************
* FUNCTION y_yuma_arp_arp_static_arps_static_arp_edit
* 
* Edit database object callback
* Path: /arp/static-arps/static-arp
* Add object instrumentation in COMMIT phase.
* 
* INPUTS:
*     see agt/agt_cb.h for details
* 
* RETURNS:
*     error status
********************************************************************/
static status_t
    y_yuma_arp_arp_static_arps_static_arp_edit (
        ses_cb_t *scb,
        rpc_msg_t *msg,
        agt_cbtyp_t cbtyp,
        op_editop_t editop,
        val_value_t *newval,
        val_value_t *curval)
{
    status_t res;
    val_value_t *errorval, *ipval, *macval;
    const xmlChar *errorstr;

    res = NO_ERR;
    errorval = NULL;
    errorstr = NULL;
    if (LOGDEBUG) {
        log_debug(
           "\nEnter y_yuma_arp_arp_static_arps_static_arp_edit callback "
           "for %s phase", agt_cbtype_name(cbtyp));
    }

    /* remove the next line if newval is used */
    /*(void)newval; */

    /* remove the next line if curval is used */
    /* (void)curval; */

    switch (cbtyp) {
    case AGT_CB_VALIDATE:
        /* description-stmt validation here */
        break;
    case AGT_CB_APPLY:
        /* database manipulation done here */
        break;
    case AGT_CB_COMMIT:
        /* device instrumentation done here */
	if(editop != OP_EDITOP_DELETE) {

	    ipval = val_find_child(newval,
		    y_yuma_arp_M_yuma_arp, 
		    y_yuma_arp_N_ip_address);

	    macval = val_find_child(newval, 
		    y_yuma_arp_M_yuma_arp, 
		    y_yuma_arp_N_mac_address);

	    if(ipval && macval) {
		modify_arp(ipval, macval, ARP_ADD, &res);
	    }
	}
        switch (editop) {
        case OP_EDITOP_LOAD:
            break;
        case OP_EDITOP_MERGE:
            break;
        case OP_EDITOP_REPLACE:
            break;
        case OP_EDITOP_CREATE:
            break;
        case OP_EDITOP_DELETE:
	    
	    ipval = val_find_child(curval,
		    y_yuma_arp_M_yuma_arp, 
		    y_yuma_arp_N_ip_address);

	    macval = val_find_child(curval, 
		    y_yuma_arp_M_yuma_arp, 
		    y_yuma_arp_N_mac_address);

	    if(ipval && macval) {
		modify_arp(ipval, macval, ARP_DEL, &res);
	    }
            
	    break;
        default:
            res = SET_ERROR(ERR_INTERNAL_VAL);
        }
        break;
    case AGT_CB_ROLLBACK:
        /* undo device instrumentation here */
        break;
    default:
        res = SET_ERROR(ERR_INTERNAL_VAL);
    }

    /* if error: set the res, errorstr, and errorval parms */
    if (res != NO_ERR) {
        agt_record_error(
            scb,
            &msg->mhdr,
            NCX_LAYER_CONTENT,
            res,
            NULL,
            NCX_NT_STRING,
            errorstr,
            NCX_NT_VAL,
            errorval);
    }
    
    return res;

} /* y_yuma_arp_arp_static_arps_static_arp_edit */


/********************************************************************
* FUNCTION y_yuma_arp_arp_static_arps_edit
* 
* Edit database object callback
* Path: /arp/static-arps
* Add object instrumentation in COMMIT phase.
* 
* INPUTS:
*     see agt/agt_cb.h for details
* 
* RETURNS:
*     error status
********************************************************************/
static status_t
    y_yuma_arp_arp_static_arps_edit (
        ses_cb_t *scb,
        rpc_msg_t *msg,
        agt_cbtyp_t cbtyp,
        op_editop_t editop,
        val_value_t *newval,
        val_value_t *curval)
{
    status_t res;
    val_value_t *errorval;
    const xmlChar *errorstr;

    res = NO_ERR;
    errorval = NULL;
    errorstr = NULL;
    if (LOGDEBUG) {
        log_debug(
           "\nEnter y_yuma_arp_arp_static_arps_edit callback for %s phase",
          agt_cbtype_name(cbtyp));
    }

    /* remove the next line if newval is used */
    (void)newval;

    /* remove the next line if curval is used */
    (void)curval;

    switch (cbtyp) {
    case AGT_CB_VALIDATE:
        /* description-stmt validation here */
        break;
    case AGT_CB_APPLY:
        /* database manipulation done here */
        break;
    case AGT_CB_COMMIT:
        /* device instrumentation done here */
        switch (editop) {
        case OP_EDITOP_LOAD:
            break;
        case OP_EDITOP_MERGE:
            break;
        case OP_EDITOP_REPLACE:
            break;
        case OP_EDITOP_CREATE:
            break;
        case OP_EDITOP_DELETE:
            break;
        default:
            res = SET_ERROR(ERR_INTERNAL_VAL);
        }
        break;
    case AGT_CB_ROLLBACK:
        /* undo device instrumentation here */
        break;
    default:
        res = SET_ERROR(ERR_INTERNAL_VAL);
    }

    /* if error: set the res, errorstr, and errorval parms */
    if (res != NO_ERR) {
        agt_record_error(
            scb,
            &msg->mhdr,
            NCX_LAYER_CONTENT,
            res,
            NULL,
            NCX_NT_STRING,
            errorstr,
            NCX_NT_VAL,
            errorval);
    }
    
    return res;

} /* y_yuma_arp_arp_static_arps_edit */


/********************************************************************
 * FUNCTION y_yuma_arp_arp_dynamic_arps_get
 * 
 * Get database object callback
 * Path: /yuma-arp/dynamic-arps
 * Fill in 'dstval' contents
 * TBD: automatic get-callback registration
 * FOR NOW: use agt_make_virtual_leaf to 
 * register this get callback fn
 * 
 * INPUTS:
 *     see ncx/getcb.h for details
 * 
 * RETURNS:
 *     error status
 ********************************************************************/
static status_t
y_yuma_arp_arp_dynamic_arps_get (
	ses_cb_t *scb,
	getcb_mode_t cbmode,
	const val_value_t *virval,
	val_value_t *dstval)
{
    status_t res;
    FILE * file;
    xmlChar * buffer, *currChar, *ip_address, *mac_address;
    int linecount;
    boolean done;

    res = NO_ERR;
    buffer = NULL;
    counter++;

    if (LOGDEBUG) {
	log_debug("\nEnter y_yuma_arp_arp_dynamic_arps_get callback");
    }

    /* remove the next line if scb is used */
    (void)scb;

    /* remove the next line if virval is used */
    (void)virval;

    if (cbmode != GETCB_GET_VALUE) {
	return ERR_NCX_OPERATION_NOT_SUPPORTED;
    }

    /* open the /proc/net/arp file for reading */
    file = fopen("/proc/net/arp", "r");
    if (file == NULL) {
	return errno_to_status();
    }

    /* get a file read line buffer */
    buffer = m__getMem(NCX_MAX_LINELEN);
    if (buffer == NULL) {
	fclose(file);
	return ERR_INTERNAL_MEM;
    }

    ip_address = m__getMem(ADDRESS_SIZE);
    if (ip_address == NULL) {
	fclose(file);
	m__free(buffer);
	return ERR_INTERNAL_MEM;
    }

    mac_address = m__getMem(ADDRESS_SIZE);
    if (mac_address == NULL) {
	fclose(file);
	m__free(buffer);
	m__free(ip_address);
	return ERR_INTERNAL_MEM;
    }

    done = FALSE;
    linecount = 0;

    /* loop through the file until done */
    while (!done) {

	currChar = (xmlChar *)
	    fgets((char *)buffer, NCX_MAX_LINELEN, file);

	if (currChar == NULL) {
	    done = TRUE;
	    continue;
	} else {
	    linecount++;
	}

	if (linecount < 2) {
	    /* skip first line */
	    continue;
	} 

	/* get IP and MAC from the line */
	res = parse_buffer(currChar, ip_address, mac_address);

	/* inserting the new entry values to the list */
	if (res == NO_ERR) {
	    (void)make_arp_entry(dstval, ip_address, mac_address, &res);
        }
    }

    m__free(mac_address);
    m__free(ip_address);
    m__free(buffer);
    fclose(file);

    return NO_ERR;

} /* y_yuma_arp_arp_dynamic_arps_get */


/********************************************************************
* FUNCTION y_yuma_arp_arp_mro
* 
* Make read-only child nodes
* Path: /arp
* 
* INPUTS:
*     parentval == the parent struct to use for new child nodes
* 
* RETURNS:
*     error status
********************************************************************/
static status_t
    y_yuma_arp_arp_mro (val_value_t *parentval)
{
    status_t res;
    val_value_t *dynamic_arp_val;
    obj_template_t * dynamic_arp_obj;

    res = NO_ERR;

    /* container arp not handled!!! */
    
    dynamic_arp_obj = obj_find_template(obj_get_datadefQ(parentval->obj), 
	    y_yuma_arp_M_yuma_arp, 
	    y_yuma_arp_N_dynamic_arps);
    if (!dynamic_arp_obj) {
	return SET_ERROR(ERR_NCX_DEF_NOT_FOUND);
    }

    dynamic_arp_val = val_new_value();
    if (!dynamic_arp_val) {
	return ERR_INTERNAL_MEM;
    }
    val_init_virtual(dynamic_arp_val, 
	    y_yuma_arp_arp_dynamic_arps_get, dynamic_arp_obj);
    val_add_child(dynamic_arp_val, parentval);

    return res;

} /* y_yuma_arp_arp_mro */


/********************************************************************
* FUNCTION y_yuma_arp_arp_edit
* 
* Edit database object callback
* Path: /arp
* Add object instrumentation in COMMIT phase.
* 
* INPUTS:
*     see agt/agt_cb.h for details
* 
* RETURNS:
*     error status
********************************************************************/
static status_t
    y_yuma_arp_arp_edit (
        ses_cb_t *scb,
        rpc_msg_t *msg,
        agt_cbtyp_t cbtyp,
        op_editop_t editop,
        val_value_t *newval,
        val_value_t *curval)
{
    status_t res;
    val_value_t *errorval;
    const xmlChar *errorstr;

    res = NO_ERR;
    errorval = NULL;
    errorstr = NULL;
    if (LOGDEBUG) {
        log_debug("\nEnter y_yuma_arp_arp_edit callback for %s phase",
            agt_cbtype_name(cbtyp));
    }

    switch (cbtyp) {
    case AGT_CB_VALIDATE:
        /* description-stmt validation here */
        break;
    case AGT_CB_APPLY:
        /* database manipulation done here */
        break;
    case AGT_CB_COMMIT:
        /* device instrumentation done here */
        switch (editop) {
        case OP_EDITOP_LOAD:
            break;
        case OP_EDITOP_MERGE:
            break;
        case OP_EDITOP_REPLACE:
            break;
        case OP_EDITOP_CREATE:
            break;
        case OP_EDITOP_DELETE:
            break;
        default:
            res = SET_ERROR(ERR_INTERNAL_VAL);
        }

        if (res == NO_ERR) {
            res = agt_check_cache(
                &arp_val,
                newval,
                curval,
                editop);
        }
        
        if (res == NO_ERR && curval == NULL) {
            res = y_yuma_arp_arp_mro(newval);
        }
        break;
    case AGT_CB_ROLLBACK:
        /* undo device instrumentation here */
        break;
    default:
        res = SET_ERROR(ERR_INTERNAL_VAL);
    }

    /* if error: set the res, errorstr, and errorval parms */
    if (res != NO_ERR) {
        agt_record_error(
            scb,
            &msg->mhdr,
            NCX_LAYER_CONTENT,
            res,
            NULL,
            NCX_NT_STRING,
            errorstr,
            NCX_NT_VAL,
            errorval);
    }
    
    return res;

} /* y_yuma_arp_arp_edit */


/********************************************************************
* FUNCTION y_yuma_arp_init_static_vars
* 
* initialize module static variables
* 
********************************************************************/
static void
    y_yuma_arp_init_static_vars (void)
{
    yuma_arp_mod = NULL;
    arp_obj = NULL;
    arp_val = NULL;

    /* init your static variables here */
    proc_net_arp_ok = 0;

} /* y_yuma_arp_init_static_vars */

#endif  /* END ifdef BUILD_ARP */


/********************************************************************
* FUNCTION y_yuma_arp_init
* 
* initialize the yuma-arp server instrumentation library
* 
* INPUTS:
*    modname == requested module name
*    revision == requested version (NULL for any)
* 
* RETURNS:
*     error status
********************************************************************/
status_t
    y_yuma_arp_init (
        const xmlChar *modname,
        const xmlChar *revision)
{
#ifdef BUILD_ARP
    agt_profile_t *agt_profile;
    status_t res;
    FILE    *testfile;

    y_yuma_arp_init_static_vars();

    /* change if custom handling done */
    if (xml_strcmp(modname, y_yuma_arp_M_yuma_arp)) {
        return ERR_NCX_UNKNOWN_MODULE;
    }

    if (revision && xml_strcmp(revision, y_yuma_arp_R_yuma_arp)) {
        return ERR_NCX_WRONG_VERSION;
    }

    agt_profile = agt_get_profile();

    /* open the /proc/net/arp file for reading */
    testfile = fopen("/proc/net/arp", "r");
    if (testfile == NULL) {
        log_info("\nSkipping yuma-arp module: Cannot open /proc/net/arp file");
        return NO_ERR;
    } else {
        fclose(testfile);
        proc_net_arp_ok = 1;
    }

    res = ncxmod_load_module(
        y_yuma_arp_M_yuma_arp,
        y_yuma_arp_R_yuma_arp,
        &agt_profile->agt_savedevQ,
        &yuma_arp_mod);
    if (res != NO_ERR) {
        return res;
    }
    
    arp_obj = ncx_find_object(
        yuma_arp_mod,
        y_yuma_arp_N_arp);
    if (yuma_arp_mod == NULL) {
        return SET_ERROR(ERR_NCX_DEF_NOT_FOUND);
    }
    
    res = agt_cb_register_callback(
        y_yuma_arp_M_yuma_arp,
        (const xmlChar *)"/arp",
        y_yuma_arp_R_yuma_arp,
        y_yuma_arp_arp_edit);
    if (res != NO_ERR) {
        return res;
    }
    
    res = agt_cb_register_callback(
        y_yuma_arp_M_yuma_arp,
        (const xmlChar *)"/arp/arp-settings",
        y_yuma_arp_R_yuma_arp,
        y_yuma_arp_arp_arp_settings_edit);
    if (res != NO_ERR) {
        return res;
    }
    
    res = agt_cb_register_callback(
        y_yuma_arp_M_yuma_arp,
        (const xmlChar *)"/arp/arp-settings/maximum-entries",
        y_yuma_arp_R_yuma_arp,
        y_yuma_arp_arp_arp_settings_maximum_entries_edit);
    if (res != NO_ERR) {
        return res;
    }
    
    res = agt_cb_register_callback(
        y_yuma_arp_M_yuma_arp,
        (const xmlChar *)"/arp/arp-settings/validity-timeout",
        y_yuma_arp_R_yuma_arp,
        y_yuma_arp_arp_arp_settings_validity_timeout_edit);
    if (res != NO_ERR) {
        return res;
    }
    
    res = agt_cb_register_callback(
        y_yuma_arp_M_yuma_arp,
        (const xmlChar *)"/arp/static-arps",
        y_yuma_arp_R_yuma_arp,
        y_yuma_arp_arp_static_arps_edit);
    if (res != NO_ERR) {
        return res;
    }
    
    res = agt_cb_register_callback(
        y_yuma_arp_M_yuma_arp,
        (const xmlChar *)"/arp/static-arps/static-arp",
        y_yuma_arp_R_yuma_arp,
        y_yuma_arp_arp_static_arps_static_arp_edit);
    if (res != NO_ERR) {
        return res;
    }
    
    res = agt_cb_register_callback(
        y_yuma_arp_M_yuma_arp,
        (const xmlChar *)"/arp/static-arps/static-arp/ip-address",
        y_yuma_arp_R_yuma_arp,
        y_yuma_arp_arp_static_arps_static_arp_ip_address_edit);
    if (res != NO_ERR) {
        return res;
    }
    
    res = agt_cb_register_callback(
        y_yuma_arp_M_yuma_arp,
        (const xmlChar *)"/arp/static-arps/static-arp/mac-address",
        y_yuma_arp_R_yuma_arp,
        y_yuma_arp_arp_static_arps_static_arp_mac_address_edit);
    if (res != NO_ERR) {
        return res;
    }
    
    /* put your module initialization code here */
    
    return res;
#else
    (void)modname;
    (void)revision;
    return NO_ERR;
#endif

} /* y_yuma_arp_init */


/********************************************************************
* FUNCTION y_yuma_arp_init2
* 
* SIL init phase 2: non-config data structures
* Called after running config is loaded
* 
* RETURNS:
*     error status
********************************************************************/
status_t
    y_yuma_arp_init2 (void)
{
#ifdef BUILD_ARP
    status_t res = NO_ERR;
    boolean  added = FALSE;

    if (!proc_net_arp_ok) {
        return res;
    }

    arp_val = agt_add_top_node_if_missing(yuma_arp_mod, y_yuma_arp_N_arp,
                                          &added, &res);
    if (res != NO_ERR || arp_val == NULL) {
        return res;
    }

    if (added) {
        /* just the top node was created, instead of going through
         * the SIL edit callback, so make-read-only was not called
         */
        res = y_yuma_arp_arp_mro(arp_val);
    }
    return res;
#else
    return NO_ERR;
#endif
} /* y_yuma_arp_init2 */


/********************************************************************
* FUNCTION y_yuma_arp_cleanup
*    cleanup the server instrumentation library
* 
********************************************************************/
void
    y_yuma_arp_cleanup (void)
{
#ifdef BUILD_ARP
    if (!proc_net_arp_ok) {
        return;
    }

    agt_cb_unregister_callbacks(
        y_yuma_arp_M_yuma_arp,
        (const xmlChar *)"/arp");

    agt_cb_unregister_callbacks(
        y_yuma_arp_M_yuma_arp,
        (const xmlChar *)"/arp/arp-settings");

    agt_cb_unregister_callbacks(
        y_yuma_arp_M_yuma_arp,
        (const xmlChar *)"/arp/arp-settings/maximum-entries");

    agt_cb_unregister_callbacks(
        y_yuma_arp_M_yuma_arp,
        (const xmlChar *)"/arp/arp-settings/validity-timeout");

    agt_cb_unregister_callbacks(
        y_yuma_arp_M_yuma_arp,
        (const xmlChar *)"/arp/static-arps");

    agt_cb_unregister_callbacks(
        y_yuma_arp_M_yuma_arp,
        (const xmlChar *)"/arp/static-arps/static-arp");

    agt_cb_unregister_callbacks(
        y_yuma_arp_M_yuma_arp,
        (const xmlChar *)"/arp/static-arps/static-arp/ip-address");

    agt_cb_unregister_callbacks(
        y_yuma_arp_M_yuma_arp,
        (const xmlChar *)"/arp/static-arps/static-arp/mac-address");

    /* put your cleanup code here */
#endif
    
} /* y_yuma_arp_cleanup */

/* END yuma_arp.c */
