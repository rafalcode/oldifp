/* $Id: ifp_routines.h,v 1.24 2005/12/10 09:03:59 yamajun Exp $ */

#ifndef IFPLINE_IFP_ROUTINES_H
#define IFPLINE_IFP_ROUTINES_H

#include <usb.h>

#define iRiver_Vendor 0x4102

#define IS_IFP		0x0
#define IS_MC		0x1
#define MC_INFO		"!!!INFO!!!"

extern int ifp_put(usb_dev_handle *dh, int argc, char *argv[]);
extern int ifp_upload(usb_dev_handle *dh, int argc, char *argv[]);
extern int ifp_upload_file(usb_dev_handle *dh, int flag, const char *name, FILE *f);
extern int ifp_get(usb_dev_handle *dh, int argc, char *argv[]);
extern int ifp_download(usb_dev_handle *dh, int argc, char *argv[]);
extern int ifp_download_file(usb_dev_handle *dh, int flag, const char *name, const char *toname);

extern int ifp_rm(usb_dev_handle *dh, int flag, int argc, char *argv[]);
extern int ifp_mkdir(usb_dev_handle *dh, int flag, int argc, char *argv[]);
extern int ifp_rmdir(usb_dev_handle *dh, int flag, int argc, char *argv[]);

extern int ifp_list_dir_printf(usb_dev_handle *dh, const char *name, int format, int recurrent);

extern int ifp_get_capacity(usb_dev_handle *dh);
extern int ifp_get_free(usb_dev_handle *dh);
extern int ifp_battery_condition(usb_dev_handle *dh);
extern int ifp_type_string(usb_dev_handle *dh, char *string);
extern int ifp_firmware_version(usb_dev_handle *dh);
extern int ifp_check_connect(usb_dev_handle *dh);

extern int ifp_format(usb_dev_handle *dh);
extern int ifp_firmware_update(usb_dev_handle *dh, int argc, char *argv[]);

extern int ifp_get_tuner_preset(usb_dev_handle *dh, int argc, char *argv[]);
extern int ifp_set_tuner_preset(usb_dev_handle *dh, int argc, char *argv[]);

extern void info(usb_dev_handle *dh, FILE *f, int head);

#endif	// IFPLINE_IFP_ROUTINES_H

