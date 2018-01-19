/*
  Copyright (c) 2018 Francesco Rana
 
  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:
 
  The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.
 
  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
  THE SOFTWARE.

  Work derived from Xadow_GPS_v2_test/test.c by WayenWeng
*/

#include "bt.h"
#include "vmlog.h"

VMINT g_tid_switch = -1; // timer id used to post BT process
VMINT g_btcm_hdl = -1; // handle of BT service
VMINT g_btspp_hdl = -1; // handle of SPP service

static vm_bt_cm_bt_address_t g_btspp_addr; // Store BT mac address of BT_NAME device we found during search process
static VMINT g_btspp_min_buf_size;  // size of buffer
static void *g_btspp_buf; // buffer that store SPP received data
static VMINT g_b_server_find; // if BT_NAME device is founded or not duing search process

VM_BT_SPP_CONNECTION_ID ble_spp_id = 0;

unsigned char bt_open = 0;


// SPP servie callback handler
void app_btspp_cb(VM_BT_SPP_EVENT evt, vm_bt_spp_event_cntx_t* param, void* user_data)
{
    vm_bt_spp_event_cntx_t *cntx = (vm_bt_spp_event_cntx_t*)param;
    VMINT ret;
    switch(evt){
        case VM_BT_SPP_EVENT_AUTHORIZE:
        {
            memset(&g_btspp_addr, 0, sizeof(g_btspp_addr));
            ret = vm_bt_spp_get_device_address(cntx->connection_id, &g_btspp_addr);
            ret = vm_bt_spp_accept(cntx->connection_id, g_btspp_buf, g_btspp_min_buf_size, g_btspp_min_buf_size);
        }
        break;

        case VM_BT_SPP_EVENT_READY_TO_WRITE:
        {
        	// write SPP_DATA example string to remote side
        }
        break;

        case VM_BT_SPP_EVENT_READY_TO_READ:
        // read data from remote side and print it out to log
        ret = vm_bt_spp_read(cntx->connection_id, g_btspp_buf, g_btspp_min_buf_size);
        if (ret > 0)
        {
            // log the received data
            ((VMCHAR*)g_btspp_buf)[ret] = 0;
            vm_log_debug("BTSPP vm_btspp_read[%s]", g_btspp_buf);
            ret = vm_bt_spp_write(cntx->connection_id, SPP_DATA, strlen(SPP_DATA));
            ble_spp_id = cntx->connection_id;
            vm_log_info("cntx->connection_id is %d", ble_spp_id);
            vm_log_info("g_btspp_buf[0] is %d", ((VMCHAR*)g_btspp_buf)[0]);
            VMUINT8 bleData = ((VMCHAR*)g_btspp_buf)[0];
            //if(bleData == 49)gps_open_flag = 1;
            //if(bleData == 48)gps_open_flag = 0;
        }
        break;

        case VM_BT_SPP_EVENT_DISCONNECT:
        // Close SPP
        ret = vm_bt_spp_close(g_btspp_hdl);
        if (g_btspp_buf)
        {
            vm_free(g_btspp_buf);
            g_btspp_buf = NULL;
        }
        g_b_server_find = 0;
        // turn off BT
        ret = vm_bt_cm_switch_off();

        bt_open = 1;
        break;
    }
}


// Init SPP servie and related resources
static void app_btspp_start(void){
    VMINT result;
    VMUINT evt_mask = VM_BT_SPP_EVENT_START	 |
        VM_BT_SPP_EVENT_BIND_FAIL	 |
        VM_BT_SPP_EVENT_AUTHORIZE	 |
        VM_BT_SPP_EVENT_CONNECT	 |
        VM_BT_SPP_EVENT_SCO_CONNECT	|
        VM_BT_SPP_EVENT_READY_TO_WRITE	|
        VM_BT_SPP_EVENT_READY_TO_READ	|
        VM_BT_SPP_EVENT_DISCONNECT	 |
        VM_BT_SPP_EVENT_SCO_DISCONNECT;

    g_btspp_hdl = vm_bt_spp_open(evt_mask, app_btspp_cb, NULL);
    if(g_btspp_hdl < 0)
    {
        return;
    }
    result = vm_bt_spp_set_security_level(g_btspp_hdl, VM_BT_SPP_SECURITY_NONE);
    g_btspp_min_buf_size = vm_bt_spp_get_min_buffer_size();

    g_btspp_buf = vm_calloc(g_btspp_min_buf_size);
    g_btspp_min_buf_size = g_btspp_min_buf_size / 2;
    result = vm_bt_spp_bind(g_btspp_hdl, UUID);
    if(result < 0)
    {
        vm_bt_spp_close(g_btspp_hdl);
        return;
    }
}

// BT servie callback handler
void app_btcm_cb(VMUINT evt, void * param, void * user_data)
{
    VMINT ret;
    switch (evt){
        case VM_BT_CM_EVENT_ACTIVATE:
        {
            //After activated, continue to scan for devices
            vm_bt_cm_device_info_t dev_info = {0};

            vm_bt_cm_activate_t *active = (vm_bt_cm_activate_t *)param;
            // set BT device host name
            ret = vm_bt_cm_set_host_name((VMUINT8*)BT_NAME);
            // display host info
            ret = vm_bt_cm_get_host_device_info(&dev_info);
            vm_log_info("BTCM vm_btcm_get_host_dev_info [%d]", ret);
            vm_log_info("BTCM vm_btcm_get_host_dev_info[%s][0x%02x:%02x:%02x:%02x:%02x:%02x]", dev_info.name,
                ((dev_info.device_address.nap & 0xff00) >> 8),
                (dev_info.device_address.nap & 0x00ff),
                (dev_info.device_address.uap),
                ((dev_info.device_address.lap & 0xff0000) >> 16),
                ((dev_info.device_address.lap & 0x00ff00) >> 8),
                (dev_info.device_address.lap & 0x0000ff));
            // set bt device as visibility
            ret = vm_bt_cm_set_visibility(VM_BT_CM_VISIBILITY_ON);
            // init SPP services
            app_btspp_start();
            break;
        }

        case VM_BT_CM_EVENT_DEACTIVATE:
        {
            ret = vm_bt_cm_exit(g_btcm_hdl);
            g_btcm_hdl = -1;
            break;
        }

        case VM_BT_CM_EVENT_SET_VISIBILITY:
        {
            vm_bt_cm_device_info_t dev_info = {0};

            vm_bt_cm_set_visibility_t *visi = (vm_bt_cm_set_visibility_t *)param;
            vm_log_info("BTCM VM_BT_CM_EVENT_SET_VISIBILITY hdl[%d] rst[%d]", visi->handle, visi->result);
            break;
        }

        default:{
            break;
        }
    }
}


// Init BT servie and turn on BT if necessary
void app_btcm_start(void)
{
    VMINT ret;
    g_btcm_hdl = vm_bt_cm_init(
        app_btcm_cb,
        VM_BT_CM_EVENT_ACTIVATE |
        VM_BT_CM_EVENT_DEACTIVATE |
        VM_BT_CM_EVENT_SET_VISIBILITY |
        VM_BT_CM_EVENT_SET_NAME,
        NULL);

    ret = vm_bt_cm_get_power_status();
    if (VM_BT_CM_POWER_OFF == ret)
    {
    	// Turn on BT if not yet on
        ret = vm_bt_cm_switch_on();
    }
    else if (VM_BT_CM_POWER_ON == ret)
    {
    	// if BT is already on
        vm_bt_cm_device_info_t dev_info = {0};
        // set bt device host name
        ret = vm_bt_cm_set_host_name((VMUINT8*)BT_NAME);
        ret = vm_bt_cm_get_host_device_info(&dev_info);
        vm_log_info("BTCM vm_btcm_get_host_dev_info[%s][0x%02x:%02x:%02x:%02x:%02x:%02x]", dev_info.name,
            ((dev_info.device_address.nap & 0xff00) >> 8),
            (dev_info.device_address.nap & 0x00ff),
            (dev_info.device_address.uap),
            ((dev_info.device_address.lap & 0xff0000) >> 16),
            ((dev_info.device_address.lap & 0x00ff00) >> 8),
            (dev_info.device_address.lap & 0x0000ff));

        // set bt device as visibility
        ret = vm_bt_cm_set_visibility(VM_BT_CM_VISIBILITY_ON);

        // init SPP services
        app_btspp_start();
    }
}
