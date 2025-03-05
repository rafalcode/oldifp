/* $Id: ifp_routines.c,v 1.59 2005/12/03 16:55:06 yamajun Exp $ */

static const char rcsid[] = "$Id: ifp_routines.c,v 1.59 2005/12/03 16:55:06 yamajun Exp $";

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <errno.h>
#include <fts.h>
#include <unistd.h>

#include "unicodehack.h"
#include "ifp_routines.h"

#define IFP_REQ_TYPE			0xc0
#define IFP_GET_CAPACITY_REQUEST	0x14
#define IFP_GET_FREE_REQUEST		0x15
#define IFP_SEND_DATA			0x18
#define IFP_LS				0x0f
#define IFP_LS_NEXT			0x10
#define IFP_LS_LEAVE			0x11
#define IFP_GET_FILE_INFO_SIZE		0x0b
#define IFP_FILE_INFO			0x05
#define IFP_FILE_LEAVE			0x0d
#define IFP_FILE_UPLOAD			0x06
#define IFP_FILE_DOWNLOAD		0x07
#define IFP_FILE_UPLOAD_APPEND		0x08
#define IFP_FILE_DELETE			0x0e
#define IFP_DIR_DELETE			0x13
#define IFP_DIR_CREATE			0x12
#define IFP_FORMAT			0x16
#define IFP_FIRMWARE_UPDATE		0x17
#define IFP_GET_PRESET			0x1d
#define IFP_SET_PRESET			0x1e
#define IFP_02_COMMAND			0x02
#define IFP_02_BATTERY			0x08
#define IFP_02_STRING			0x00
#define IFP_02_FIRMWARE			0x03
#define IFP_02_UPLOAD_FLUSH		0x06

#define IFP_FILE			1
#define IFP_DIR				2

#define TOUT 1000
#define FIRMUPDATE_TIMEOUT		5

#define FIRMWARE_HEADER_SIZE		4

#define IFP_FILE_BUFLEN			0x4000
#define IFP_BULK_MAXPATHLEN		0x400
#define IFP_MAXPATHLEN			0x200
#define IFP_LSBUFLEN			0x100

#define IFP_PRESETLEN			0x100

int ifp_upload_dir(usb_dev_handle *dh, const char *from, const char *name);
int ifp_download_dir(usb_dev_handle *dh, const char *fromdir, const char *todir);
int ifp_delete_file(usb_dev_handle *dh, const char *name);
int ifp_delete_recursive(usb_dev_handle *dh, char *name);
int ifp_delete_dir(usb_dev_handle *dh, char *name);
int ifp_make_dir(usb_dev_handle *dh, char *name);

int ifp_ctl_ret(unsigned char *control);
int ifp_ok(unsigned char *control);
int ifp_send_filename(usb_dev_handle *dh, const char *name);
int ifp_send_data(usb_dev_handle *dh, char *data, size_t size);
int filestat(const char *name);
int ifp_empty_dir(usb_dev_handle *dh, const char *name);
int ifp_isdir(usb_dev_handle *dh, const char *name);
int push_dir(char *name);
int pop_dir(char *name);
int comparefts(const FTSENT **a, const FTSENT **b);
char *get_basename(const char *path);
char *ifp_strcasestr(const char *base, const char *target);
int change_suffix(char *name, const char *newsuffix);
void normalize_path(char *path, size_t size);
void remove_double_slash(char *path, size_t size);
void slash2backslash(char *dest, const char *src, size_t count);
int ifp_ctl_datasize(unsigned char *control);
char *dump_control_regs(const char *control, int length);
int safe_suffix(usb_dev_handle *dh, const char *name);
int firmware_check_ifp(const char *firm_name);
int firmware_check_n10(const char *firm_name);
int ifp_file_exists(usb_dev_handle *dh, const char *name);
int get_dirname(char *dest, const char *path);


typedef struct dirlists {
    struct dirlists *next;
    char *name;
} dirlist;

dirlist *list = NULL;

int IFP_BULK_TO;
int IFP_BULK_FROM;
int isOldIFP;				    // 0: new, other: old

/********* public interface functions **********/
int ifp_put(usb_dev_handle *dh, int argc, char *argv[]) {
    struct stat status;
    char *nargv[3];

    if (argc < 2) {
	printf("usage: put localfile\n");
	printf("       put localdir\n");
	return -1;
    }

    nargv[0] = argv[0];
    nargv[1] = argv[1];

    if (stat(argv[1], &status) != -1) {
	if (S_ISDIR(status.st_mode)) {
	    nargv[2] = "/";
	} else {
	    nargv[2] = get_basename(argv[1]);
	}
    } else {
	perror("Unable to upload file");
	return -1;
    }

    return ifp_upload(dh, 3, nargv);
}

int ifp_upload(usb_dev_handle *dh, int argc, char *argv[]) {
    FILE *fp;
    int retval = 0;
    char ifp_path[IFP_MAXPATHLEN];
    char path_buf[PATH_MAX];

    if (argc < 3) {
	printf("usage: upload localfile ifptarget\n");
	printf("       upload localdir  ifpdir\n");
	return -1;
    }

    switch (filestat(argv[1])) {
    case 0:	// file not exist
	fprintf(stderr, "ifp upload: %s: not found.\n", argv[1]);
	return -1;
	break;

    case 1:	// regular file
	if ( (fp = fopen(argv[1], "r")) == NULL) {
	    perror(argv[1]);
	    return -1;
	} else {
	    strncpy(ifp_path, argv[2], IFP_MAXPATHLEN - 1);
	    normalize_path(ifp_path, IFP_MAXPATHLEN);

	    if (ifp_isdir(dh, ifp_path)) {
		// file to dir
		snprintf(ifp_path, IFP_MAXPATHLEN, "%s/%s", argv[2], get_basename(argv[1]));
		normalize_path(ifp_path, IFP_MAXPATHLEN);

		retval = ifp_upload_file(dh, IS_IFP, ifp_path, fp);

	    } else {
		retval = ifp_upload_file(dh, IS_IFP, ifp_path, fp);  // file to file
	    }
	    fclose(fp);
	}
	break;

    case 2:	// directory
	strncpy(path_buf, argv[1], PATH_MAX - 1);
	normalize_path(path_buf, PATH_MAX);

	strncpy(ifp_path, argv[2], IFP_MAXPATHLEN - 1);
	normalize_path(ifp_path, IFP_MAXPATHLEN);

	retval = ifp_upload_dir(dh, path_buf, ifp_path);   // dir to dir
	break;

    default:
	return -1;
	break;
    }

    if (retval <= -2) {
	fprintf(stderr, "ifp upload: Not enough space on device.\n");
	return -1;
    }
	
    return retval;
}

// NOTE: "name" need normalize.
int ifp_upload_file(usb_dev_handle *dh, int flag, const char *name, FILE *f) {
    int i,sum = 0;
    unsigned char buf[IFP_FILE_BUFLEN];
    unsigned char ctl[4];
    unsigned char ctl8[8];
    int filesize = 0;	// this is a hack, original manager sends file size, this works
    struct stat stat;

    fstat(fileno(f),&stat);
    if (stat.st_size > ifp_get_free(dh)) {
	// not enough space
	fprintf(stderr, "%s: Not enough space.  Cannot upload.\n", name);
	return -2;
    } else if (strlen(name) >= IFP_MAXPATHLEN) {
	// Pathname too long
	fprintf(stderr, "%s: Pathname too long.  Cannot upload.\n", name);
	return -3;
    }

    rewind(f);
    ifp_send_filename(dh,name);
    usb_control_msg(dh,IFP_REQ_TYPE,IFP_FILE_UPLOAD,filesize,0,ctl,sizeof(ctl),TOUT);
    if (!ifp_ok(ctl)) {
	fprintf(stderr, "%s: Cannot upload.\n", name);
	return -1;
    }

    while (!feof(f)) {
	i = fread(buf,1,sizeof(buf),f);
	//printf("%d bytes read form file...\n",i);
	if (flag != IS_MC) {
	    printf("%d sent\r",sum);
	    fflush(stdout);
	}
	ifp_send_data(dh,buf,i);
	usb_control_msg(dh,IFP_REQ_TYPE,IFP_FILE_UPLOAD_APPEND,i,0,ctl,sizeof(ctl),TOUT);

	sum += i;
	//printf("upload-block-ctl: %d %d %d %d\n",ctl[0],ctl[1],ctl[2],ctl[3],ctl[4]);
    }
    if (flag != IS_MC)
	printf("%d sent.\n",sum);

    while (1) {
	usb_control_msg(dh, IFP_REQ_TYPE, IFP_02_COMMAND, 0,
			IFP_02_UPLOAD_FLUSH, ctl8, sizeof(ctl8), TOUT);

	usb_bulk_read(dh, IFP_BULK_FROM, ctl, sizeof(ctl), TOUT);
	if (ctl[0] != 2) break;
    }

    usb_control_msg(dh,IFP_REQ_TYPE,IFP_FILE_LEAVE,0,0,ctl,sizeof(ctl),TOUT);
    if (!ifp_ok(ctl)) return -1;
    return 0;
}

int ifp_get(usb_dev_handle *dh, int argc, char *argv[]) {
    char *nargv[3];

    if (argc < 2) {
	printf("usage: get ifpfile\n");
	printf("       get ifpdir\n");
	return -1;
    }

    nargv[0] = argv[0];
    nargv[1] = argv[1];
    nargv[2] = ".";		// set destination

    return ifp_download(dh, 3, nargv);
}

int ifp_download(usb_dev_handle *dh, int argc, char *argv[]) {
    int retval = 0;
    char ifp_path[IFP_MAXPATHLEN];
    char path_buf[PATH_MAX];

    if (argc < 3) {
	printf("usage: download ifpfile localtarget\n");
	printf("       download ifpdir  localdir\n");
	return -1;
    }

    strncpy(ifp_path, argv[1], IFP_MAXPATHLEN - 1);
    normalize_path(ifp_path, IFP_MAXPATHLEN);

    if (!ifp_file_exists(dh, ifp_path)) {
	fprintf(stderr, "ifp: No such file or directory in iFP: %s\n", ifp_path);
	return -1;
    }

    switch (filestat(argv[2])) {
    case 0: // "argv[2]" does not exist. create new file.
	// file to file
	if (ifp_isdir(dh, ifp_path)) {
	    fprintf(stderr,
		    "ifp download: Cannot copy %s to %s(%s is a directory).\n",
		    ifp_path, argv[2], ifp_path);
	    retval = -1;
	    break;
	}

	if (!safe_suffix(dh, argv[2])) {
	    break;
	}

	retval = ifp_download_file(dh, IS_IFP, ifp_path, argv[2]);
	break;

    case 1: // regular file
	// file to file(exist)
	if (ifp_isdir(dh, ifp_path)) {
	    fprintf(stderr,
		    "ifp download: Cannot copy %s to %s(%s is a directory).\n",
		    ifp_path, argv[2], ifp_path);
	    retval = -1;
	    break;
	}

	printf("WARNING: \"%s\" exist. Do you continue?: ", argv[2]);
	switch (getc(stdin)) {
	case 'Y':	/* yes */
	case 'y':
	    if (!safe_suffix(dh, argv[2])) {
		break;
	    }

	    retval = ifp_download_file(dh, IS_IFP, ifp_path, argv[2]);
	    break;
	default:	/* no */
	    retval = -1;
	    break;
	}
	break;

    case 2: // directory
	if (ifp_isdir(dh, ifp_path)) {
	    // dir to dir
	    snprintf(path_buf, PATH_MAX, "%s/%s", argv[2], get_basename(ifp_path));
	    retval = ifp_download_dir(dh, ifp_path, path_buf);

	} else {
	    // file to dir
	    snprintf(path_buf, PATH_MAX, "%s/%s", argv[2], get_basename(ifp_path));
	    if (!safe_suffix(dh, path_buf)) {
		break;
	    }
	    retval = ifp_download_file(dh, IS_IFP, ifp_path, path_buf);
	}
	break;

    default:
	return -1;
	break;
    }

    return retval;
}

// NOTE: "name" need normalize.
int ifp_download_file(usb_dev_handle *dh, int flag, const char *name, const char *toname) {
    FILE *fp;
    pid_t pid;
    int i,size,sum = 0;
    unsigned char ctl[4];
    unsigned char ctl8[8];
    unsigned char buf[IFP_FILE_BUFLEN];
    unsigned char typestr[10];
    char *newname;

    if ( (fp = fopen(toname, "w")) == NULL) {
	perror("ifp_download_file");
	return -1;
    }

    ifp_send_filename(dh,name);

    usb_control_msg(dh, IFP_REQ_TYPE, IFP_FILE_INFO, IFP_FILE, 0,
		    ctl, sizeof(ctl), TOUT);

    if (!ifp_ok(ctl) && flag != IS_MC) {
	 fprintf(stderr,"ifp download: Could not retrieve file infomation.\n");
	 return -1;
    }

    usb_control_msg(dh,IFP_REQ_TYPE,IFP_GET_FILE_INFO_SIZE,0,0,ctl8,sizeof(ctl8),TOUT);
    do {
	usb_control_msg(dh,IFP_REQ_TYPE,IFP_FILE_DOWNLOAD,sizeof(buf),0,ctl,sizeof(ctl),TOUT);
	size = ctl[0] + (ctl[1]<<8);
	if (size > 0) {
	    i = usb_bulk_read(dh,IFP_BULK_FROM,buf,size,TOUT);
	    fwrite(buf, sizeof(char), i, fp);
	    sum += i;
	    if (flag != IS_MC) {
		printf("%d received\r", sum);
		fflush(stdout);
	    }
	}
    } while (size > 0);

    fclose(fp);

    if (flag != IS_MC) {
	printf("%d received.\n", sum);
    }

    usb_control_msg(dh,IFP_REQ_TYPE,IFP_FILE_LEAVE,0,0,ctl,sizeof(ctl),TOUT);
    if (!ifp_ok(ctl)) return -1;
    
    // convert *.REC file to *.wav/*.mp3
    if ( (ifp_strcasestr(toname, ".REC") - toname) == (strlen(toname) - 4) ) {

	if ( (newname = malloc(strlen(toname)+1)) == NULL) {
	    perror("ifp_download_file");
	    return -1;
	}
	memcpy(newname, toname, strlen(toname)+1);

	ifp_type_string(dh, typestr);
	if (strstr(typestr, "IFP-1") != NULL) {	// iFP-1XX
	    // HOGE000.REC -> HOGE000.wav
	    change_suffix(newname, ".wav");

	    if ( (pid = fork()) < 0) {
		perror("ifp_download_file");
		return -1;
	    } else if (pid == 0) {
		if (execlp("ifprecconv", "ifprecconv", toname, newname, NULL) == -1)
		    perror("ifprecconv");

	    } else {
		wait(NULL);
	    }

	} else {				// iFP-3XX, iFP-5XX and newer products
	    // HOGE000.REC -> HOGE000.mp3
	    change_suffix(newname, ".mp3");
	    if (rename(toname, newname) == -1) {
		perror("ifp_download_file");
		return -1;
	    }
	}
    }
    return 0;
}

int ifp_rm(usb_dev_handle *dh, int flag, int argc, char *argv[]) {
    int retval = 0;
    char ifp_path[IFP_MAXPATHLEN];

    // for mc support
    // Note: Don't use argv[1] in mc plugin mode!
    if (flag == IS_MC) {
	if (argc < 3) {
	    return -1;
	}
	return ifp_delete_file(dh, argv[2]);
    }

    if (argc < 2) {
	printf("usage: rm [-r] file\n");
	return -1;
    } else {
	if (strcmp(argv[1], "-r") == 0) {
	    if (argc == 2) {
		printf("usage: rm [-r] file\n");
		return -1;
	    } else {
		strncpy(ifp_path, argv[2], IFP_MAXPATHLEN - 1);
		normalize_path(ifp_path, IFP_MAXPATHLEN);

		retval = ifp_delete_recursive(dh, ifp_path);
	    }

	} else {
	    strncpy(ifp_path, argv[1], IFP_MAXPATHLEN - 1);
	    normalize_path(ifp_path, IFP_MAXPATHLEN);

	    retval = ifp_delete_file(dh, ifp_path);
	}
    }

    return retval;
}

int ifp_mkdir(usb_dev_handle *dh, int flag, int argc, char *argv[]) {
    char ifp_path[IFP_MAXPATHLEN];

    if (flag == IS_MC) {
	return ifp_make_dir(dh, argv[2]);
    } else {
	strncpy(ifp_path, argv[1], IFP_MAXPATHLEN - 1);
	normalize_path(ifp_path, IFP_MAXPATHLEN);

	return ifp_make_dir(dh, ifp_path);
    }
}

int ifp_rmdir(usb_dev_handle *dh, int flag, int argc, char *argv[]) {
    char ifp_path[IFP_MAXPATHLEN];

    if (flag == IS_MC) {
	return ifp_delete_dir(dh, argv[2]);
    } else {
	strncpy(ifp_path, argv[1], IFP_MAXPATHLEN - 1);
	normalize_path(ifp_path, IFP_MAXPATHLEN);

	return ifp_delete_dir(dh, ifp_path);
    }
}

int ifp_list_dir_printf(usb_dev_handle *dh, const char *name, int format, int recurrent) {
    unsigned char ctl[4];
    unsigned char ctl8[8];
    char lsbuf[IFP_LSBUFLEN];
    char ascii_lsbuf[IFP_LSBUFLEN];
    char filename[IFP_MAXPATHLEN];
    char ifp_path[IFP_MAXPATHLEN];
    int ret;
    int size;

    strncpy(ifp_path, name, IFP_MAXPATHLEN - 1);
    normalize_path(ifp_path, IFP_MAXPATHLEN);

    if (ifp_send_filename(dh, ifp_path) < 0) {
	fprintf(stderr, "ifp ls: Could not send filename: %s.\n",  ifp_path);
	return -1;
    }
    
    usb_control_msg(dh, IFP_REQ_TYPE,IFP_LS, 0, 0, ctl, sizeof(ctl), TOUT);
    if (!ifp_ok(ctl)) {
	fprintf(stderr, "ifp ls: %s: No such directory in iFP.\n",  ifp_path);
	return -1;
    }

    while (1) {
	usb_control_msg(dh, IFP_REQ_TYPE, IFP_LS_NEXT, IFP_FILE | IFP_DIR, 0,
			ctl, sizeof(ctl), TOUT);
	ret = ifp_ctl_ret(ctl);

	switch (ret) {
	case 0:
	    goto LS_END;
	case 1:
	    usb_bulk_read(dh, IFP_BULK_FROM, lsbuf, sizeof(lsbuf), TOUT);
	    unicode2locale(ascii_lsbuf, IFP_LSBUFLEN, lsbuf, IFP_LSBUFLEN);

	    strncpy(filename, ifp_path, IFP_MAXPATHLEN);
	    if (strcmp(ifp_path, "/") != 0) {
		strcat(filename, "/");
	    }
	    // path/file
	    strncat(filename, ascii_lsbuf, IFP_MAXPATHLEN-strlen(filename)-1);

	    ifp_send_filename(dh, filename);
	    usb_control_msg(dh, IFP_REQ_TYPE, IFP_FILE_INFO, IFP_FILE, 0,
			    ctl, sizeof(ctl), TOUT);
	    if (!ifp_ok(ctl)) {
		fprintf(stderr,
			"ifp ls: File Information command failed. ctl = %s.\n",
			dump_control_regs(ctl, sizeof(ctl)) );
		return -1;
	    }
	    
	    usb_control_msg(dh,IFP_REQ_TYPE,IFP_GET_FILE_INFO_SIZE,0,0,ctl8,sizeof(ctl8),TOUT);
	    if (!ifp_ok(ctl8)) {
		 fprintf(stderr,
			 "ifp ls: File Information Size command failed. ctl8 = %s.\n",
			 dump_control_regs(ctl8,sizeof(ctl8)) );
		 return -1;
	    }

	    size = ctl8[4] + (ctl8[5]<<8) + (ctl8[6]<<16) + (ctl8[7]<<24);
	    if (format) {
		printf("-rw-rw-rw-  1 root  root  %7d Jan  1 1981 %s\n",
			size, filename);
	    } else {
		printf("f %s\t(size %d)\n", ascii_lsbuf, size);
	    }
	    usb_control_msg(dh,IFP_REQ_TYPE,IFP_FILE_LEAVE,0,0,ctl,sizeof(ctl),TOUT); //??????
	    break;
	case 2:
	    usb_bulk_read(dh, IFP_BULK_FROM, lsbuf, sizeof(lsbuf), TOUT);
	    unicode2locale(ascii_lsbuf, IFP_LSBUFLEN, lsbuf, IFP_LSBUFLEN);

	    strncpy(filename, ifp_path, IFP_MAXPATHLEN);
	    if (strcmp(filename, "/") != 0) strcat(filename, "/");
	    // path/file
	    strncat(filename, ascii_lsbuf, IFP_MAXPATHLEN-strlen(filename)-1);

	    if (format) {
		printf("drw-rw-rw-  1 root  root        0 Jan  1 1980 %s\n",
			filename);
	    } else {
		printf("d %s\n", ascii_lsbuf);	//XXX
	    }

	    if (recurrent) {
		push_dir(filename); // push directory with path for later use
	    }
	    break;
	default:
	    fprintf(stderr, "Unknown LS_NEXT return value = %d\n", ret);
	    return -3;
	    break;
	}
    }
    LS_END:
    usb_control_msg(dh,IFP_REQ_TYPE,IFP_LS_LEAVE,0,0,ctl,sizeof(ctl),TOUT);
    /* now go through the directories on the stack */
    while (pop_dir(filename)) {
	ifp_list_dir_printf(dh, filename, format, recurrent);
    }
    return 0;
}

int ifp_get_capacity(usb_dev_handle *dh) {
    unsigned char ctl[8];
    usb_control_msg(dh,IFP_REQ_TYPE,IFP_GET_CAPACITY_REQUEST,0,0,(char *)&ctl,sizeof(ctl),TOUT);
    if (!ifp_ok(ctl)) return -1;
    return (ctl[7]<<24) + (ctl[6]<<16) + (ctl[5]<<8) + ctl[4];
}

int ifp_get_free(usb_dev_handle *dh) {
    unsigned char ctl[8];
    usb_control_msg(dh,IFP_REQ_TYPE,IFP_GET_FREE_REQUEST,0,0,ctl,sizeof(ctl),TOUT);
    if (!ifp_ok(ctl)) return -1;
    return (ctl[7]<<24) + (ctl[6]<<16) + (ctl[5]<<8) + ctl[4];
}

/* not tested, not sure: */
int ifp_battery_condition(usb_dev_handle *dh) {
    int size = 0;
    unsigned char ctl8[8];
    unsigned char battery[256];

    usb_control_msg(dh,IFP_REQ_TYPE,IFP_02_COMMAND,0,IFP_02_BATTERY,ctl8,sizeof(ctl8),TOUT);
    if (!ifp_ok(ctl8)) {
	fprintf(stderr,
		"ifp battery: Could not get battery status. ctl8 = %s.\n",
		dump_control_regs(ctl8, sizeof(ctl8)) );
	return -1;
    }

    size = ifp_ctl_datasize(ctl8);
    usb_bulk_read(dh, IFP_BULK_FROM, battery, size, TOUT);

    return battery[0];
}

int ifp_type_string(usb_dev_handle *dh, char *string) {
    int size = 0;
    unsigned char ctl8[8];
    unsigned char model[256];

    usb_control_msg(dh,IFP_REQ_TYPE,IFP_02_COMMAND,0,IFP_02_STRING,ctl8,sizeof(ctl8),TOUT);
    if (!ifp_ok(ctl8)) {
	fprintf(stderr, "ifp typestring: Could not get model string from device. ctl8 = %s.\n",
		dump_control_regs(ctl8, sizeof(ctl8)) );
	return -1;
    }
    size = ifp_ctl_datasize(ctl8);
    usb_bulk_read(dh, IFP_BULK_FROM, model, size, TOUT);
    strncpy(string, model, size);

    return 0;
}

int ifp_firmware_version(usb_dev_handle *dh) {
    int size = 0;
    unsigned char ctl8[8];
    unsigned char fversion[256];

    usb_control_msg(dh,IFP_REQ_TYPE,IFP_02_COMMAND,0,IFP_02_FIRMWARE,ctl8,sizeof(ctl8),TOUT);
    if (!ifp_ok(ctl8)) {
	fprintf(stderr,
		"ifp firmversion: Could not get firmware version. ctl8 = %s.\n",
		dump_control_regs(ctl8, sizeof(ctl8)) );
	return -1;
    }
    size = ifp_ctl_datasize(ctl8);
    usb_bulk_read(dh, IFP_BULK_FROM, fversion, size, TOUT);

    return ((fversion[1]/16)*1000 +
	    (fversion[1]%16)*100 +
	    (fversion[0]/16)*10 +
	    (fversion[0]%16));
}

int ifp_check_connect(usb_dev_handle *dh) {
    return (ifp_battery_condition(dh) >= 0);
}

int ifp_format(usb_dev_handle *dh) {
    struct usb_device *dev = usb_device(dh);
    unsigned char ctl[4];

    printf("WARNING: Do you want to format iFP? [y/N]: ");
    switch (getc(stdin)) {
    case 'Y':	/* yes */
    case 'y':
	break;
    default:	/* no */
	return -1;
	break;
    }

    printf("Formating. Please wait...\n");
    // send control message
    usb_control_msg(dh, IFP_REQ_TYPE, IFP_FORMAT, 0, 0, ctl, sizeof(ctl), TOUT);
    // FIXME
    //printf("Done.\n");

    //return 0;
    //usb_control_msg(dh, IFP_REQ_TYPE, IFP_FIRMWARE_UPDATE, 0, 0, ctl, sizeof(ctl), TOUT);

    // close/reopen iFP for update.
    usb_release_interface(dh, dev->config->interface->altsetting->bInterfaceNumber);

//#ifdef linux 
//    usb_reset(dh);	// Don't use for Mac OS X.
//#endif

    usb_close(dh);
    sleep(FIRMUPDATE_TIMEOUT);	// need this!

    if ( (dh = usb_open(dev)) == NULL) {
	fprintf(stderr, "Unable to open iFP device.\n");
	return -1;
    }

    if (usb_claim_interface(dh, dev->config->interface->altsetting->bInterfaceNumber)) {
	fprintf(stderr, "Device is busy.  (I was unable to claim its interface.)\n");
	usb_close(dh);
	exit(1);
    }

    /*if (!ifp_check_connect(dh)) {
	fprintf(stderr, "Unable to connect to iFP.\n");
	return -1;
    }*/

    printf("Done.\n");

    return 0;
}

int ifp_firmware_update(usb_dev_handle *dh, int argc, char *argv[]) {
    struct usb_device *dev = usb_device(dh);
    FILE *fp;
    int retval;
    char buf[256];
    unsigned char ctl[4];
    char *basename;

    if (argc < 2) {
	printf("usage: firmupdate /path/to/FIRMWARE.HEX\n");
	return -1;
    } else {
	printf("WARNING: Do you want to firmware update? [y/N]: ");
	switch (getc(stdin)) {
	case 'Y':   /* yes */
	case 'y':
	    break;
	default:    /* no */
	    return -1;
	    break;
	}
    }

    if (ifp_type_string(dh, buf) < 0) {
	return -1;
    }
    if (strncmp(buf, "IFP-", 4) == 0) {
	if (firmware_check_ifp(argv[1]) < 0) {
	    fprintf(stderr, "Failed firmware consistency check.\n");
	    return -1;
	}
    } else {
	if (firmware_check_n10(argv[1]) < 0) {
	    fprintf(stderr, "Failed firmware consistency check.\n");
	    return -1;
	}
    }

    // send firmware file.
    if ( (fp = fopen(argv[1], "r")) == NULL) {
	perror(argv[1]);
	return -1;
    }

    // Send firmware file to /FIRMWARE.HEX in iFP.
    basename = get_basename(argv[1]);
    if ( (retval = ifp_upload_file(dh, IS_IFP, basename, fp)) < 0) {
	fprintf(stderr, "Failed firmware upload.\n");
	fclose(fp);
	return retval;
    }
    fclose(fp);
    printf("Please wait few minutes...\n");

    // send control message
    usb_control_msg(dh, IFP_REQ_TYPE, IFP_FIRMWARE_UPDATE, 0, 0, ctl, sizeof(ctl), TOUT);

    // close/reopen iFP for update.
    usb_release_interface(dh, dev->config->interface->altsetting->bInterfaceNumber);

//#ifdef linux 
//    usb_reset(dh);	// Don't use for Mac OS X.
//#endif

    usb_close(dh);
    sleep(FIRMUPDATE_TIMEOUT);	// need this!

    if ( (dh = usb_open(dev)) == NULL) {
	fprintf(stderr, "Unable to open iFP device.\n");
	return -1;
    }

    if (usb_claim_interface(dh, dev->config->interface->altsetting->bInterfaceNumber)) {
	fprintf(stderr, "Device is busy.  (I was unable to claim its interface.)\n");
	usb_close(dh);
	exit(1);
    }

    if (!ifp_check_connect(dh)) {
	fprintf(stderr, "Unable to connect to iFP.\n");
	return -1;
    }
    return retval;
}

int ifp_get_tuner_preset(usb_dev_handle *dh, int argc, char *argv[]) {
    FILE *fp;
    unsigned char ctl[4];
    int i;
    char buf[IFP_PRESETLEN];

    if (argc < 2) {
	printf("usage: getpreset myarea.dat\n");
	return -1;
    } else {
	if (ifp_firmware_version(dh) < 200) {
	    fprintf(stderr, "Does not support this firmware(before v2.00).\n");
	}
    }

    if ( (fp = fopen(argv[1], "w")) == NULL) {
	perror(argv[1]);
	return -1;
    }

    usb_control_msg(dh, IFP_REQ_TYPE, IFP_GET_PRESET, 0, 0, ctl, sizeof(ctl), TOUT);
    if (!ifp_ok(ctl)) return -1;

    i = usb_bulk_read(dh, IFP_BULK_FROM, buf, sizeof(buf), TOUT);
    if (fwrite(buf, 1, i, fp) < 0) {
	fprintf(stderr, "Cannot write to preset file.\n");
	return -1;
    }

    // XXX This two line is hack, but need this!
    usb_control_msg(dh, IFP_REQ_TYPE, IFP_GET_PRESET, 0, 0, ctl, sizeof(ctl), TOUT);
    i = usb_bulk_read(dh, IFP_BULK_FROM, buf, sizeof(buf), TOUT);

    fclose(fp);
    return 0;
}

int ifp_set_tuner_preset(usb_dev_handle *dh, int argc, char *argv[]) {
    FILE *fp;
    unsigned char ctl[4];
    size_t size;
    // XXX To iRiver: Why this data size is twice as received preset?
    char buf[IFP_PRESETLEN*2];

    if (argc < 2) {
	printf("usage: setpreset myarea.dat\n");
	return -1;
    } 

    ifp_type_string(dh, buf);
    // No FM station name display in iFP-1xx with old firmware(before 2.00).
    if (strstr(buf, "IFP-1") != NULL) {
	if ((ifp_firmware_version(dh) < 200)) {
	    fprintf(stderr, "Does not support this command with old firmware(before v2.00).\n");
	    return -1;
	}
    }
    
    printf("WARNING: Do you want to change FM preset? [y/N]: ");
    switch (getc(stdin)) {
    case 'Y':	/* yes */
    case 'y':
	break;
    default:	/* no */
	return -1;
	break;
    }

    if ( (fp = fopen(argv[1], "r")) == NULL) {
	perror(argv[1]);
	return -1;
    }

    if ( (size = fread(buf, 1, sizeof(buf), fp)) < 0) {
	fprintf(stderr, "Cannot read from preset file.\n");
	return -1;
    }
    ifp_send_data(dh, buf, size);

    usb_control_msg(dh, IFP_REQ_TYPE, IFP_SET_PRESET, 0, 0, ctl, sizeof(ctl), TOUT);

    fclose(fp);
    if (!ifp_ok(ctl)) return -1;

    return 0;
}

void info(usb_dev_handle *dh, FILE *f, int head) {
    int i;
    char buf[255];
    if (head) {
    fprintf(f,"### This file is virtual, it's not saved in your iFP, but generated by driver\n");
    fprintf(f,"### Following information is obtained only once per connection,\n");
    fprintf(f,"###   press enter (run) on this file to get up-to-date information!\n");
    fprintf(f,"\n");
    }
    ifp_type_string(dh,buf);
    fprintf(f,"Product:         %10s (firmware %.2f)\n",buf,ifp_firmware_version(dh)/100.0);
    i = ifp_get_capacity(dh);
    fprintf(f,"Available space: %10d (%.1fMB)\n",i,i/1024.0/1024.0);
    i = ifp_get_free(dh);
    fprintf(f,"Free space:      %10d (%.1fMB)\n",i,i/1024.0/1024.0);
    fprintf(f,"Battery status:  %10d\n",ifp_battery_condition(dh));
}


/********** internal functions **********/
// NOTE: "name" need normalize.
int ifp_upload_dir(usb_dev_handle *dh, const char *from, const char *name) {
    FILE *fp;
    FTS *ftsp;
    FTSENT *ftsentp;
    size_t basename_pos;
    int fts_options = FTS_LOGICAL;
    int retval = -1;
    char ifp_path[IFP_MAXPATHLEN];
    char *toname;
    char *argv[2] = {(char*)from, NULL};

    basename_pos = strlen(from) - strlen(get_basename(from));

    if ( (ftsp = fts_open(argv, fts_options, comparefts)) == NULL) {
	perror("fts_open");
	goto FTS_END;
    }

    while ( (ftsentp = fts_read(ftsp)) != NULL) {
	// "fromdir/file" -> "file"
	toname = ftsentp->fts_path + basename_pos;

	// "path" "file" -> "path/file"
	snprintf(ifp_path, IFP_MAXPATHLEN, "%s/%s", name, toname);

	switch (ftsentp->fts_info) {
	case FTS_F:		// normal file
	    if ( (fp = fopen(ftsentp->fts_path, "r")) == NULL) {
		perror(ftsentp->fts_path);
		goto FTS_END;
	    }

	    // -2: not enough space
	    if ((retval = ifp_upload_file(dh, IS_IFP, ifp_path, fp)) == -2) {
		fclose(fp);
		goto FTS_END;
	    }

	    fclose(fp);

	    // This is hack. But, it need for upload many files!
	    // Removed another hack that kills 7XX and 8XX series.

	    //usleep(500000);	// suspend 0.5[sec]
	    if (isOldIFP) {
		if (!ifp_check_connect(dh)) {
		    fprintf(stderr, "Unable to connect to iFP.\n");
		    return -1;
		}
	    }

	    break;

	case FTS_D:		// directory
	    retval = ifp_make_dir(dh, ifp_path);
	    break;

	default:
	    break;
	}
    }

FTS_END:
    if (fts_close(ftsp) == -1) {
	perror("fts_close");
	return -1;
    }
    return retval;
}

int ifp_download_dir(usb_dev_handle *dh, const char *fromdir, const char *todir) {
    int retval = 0;
    unsigned char inbuf[IFP_LSBUFLEN];
    unsigned char strbuf[IFP_LSBUFLEN];
    unsigned char ifp_path[IFP_MAXPATHLEN];
    unsigned char to_path[IFP_MAXPATHLEN];
    unsigned char ctl[4];

    if (mkdir(todir, (S_IRWXU | S_IRWXG | S_IRWXO)) == -1) {
	if (errno != EEXIST) {
	    perror(todir);
	    return -1;
	}
    }

    if (ifp_send_filename(dh, fromdir) < 0)
	return -1;

    usb_control_msg(dh, IFP_REQ_TYPE, IFP_LS, 0, 0, ctl, sizeof(ctl), TOUT);
    if (!ifp_ok(ctl)) {
	fprintf(stderr, "ifp download: Could not ls %s directory.\n", fromdir);
	return -1;
    }

    while (1) {
	usb_control_msg(dh, IFP_REQ_TYPE, IFP_LS_NEXT, IFP_FILE | IFP_DIR, 0,
			ctl, sizeof(ctl), TOUT);
	switch (ifp_ctl_ret(ctl)) {
	case 0:	// end
	    goto LS_END;
	    break;
	case 1:	// file
	    usb_bulk_read(dh,IFP_BULK_FROM,inbuf,sizeof(inbuf),TOUT);
	    unicode2locale(strbuf, IFP_LSBUFLEN, inbuf, IFP_LSBUFLEN);

	    // "path" "file" -> "path/file"
	    snprintf(ifp_path, IFP_MAXPATHLEN, "%s/%s", fromdir, strbuf);
	    snprintf(to_path, IFP_MAXPATHLEN, "%s/%s", todir, strbuf);

	    switch (filestat(to_path)) {
	    case 0:	// "to_path" not found;
		if (!safe_suffix(dh, to_path)) {
		    break;
		}

		retval = ifp_download_file(dh, IS_IFP, ifp_path, to_path);
		break;
	    case 1:	// "to_path" exist
		printf("WARNING: \"%s\" exist. Do you continue?: ", to_path);
		switch (getc(stdin)) {
		case 'Y':	/* yes */
		case 'y':
		    if (!safe_suffix(dh, to_path)) {
			break;
		    }

		    retval = ifp_download_file(dh, IS_IFP, ifp_path, to_path);
		    break;
		default:	/* no */
		    break;
		}   // switch (getc(stdin))
		break;

	    case 2:	// "to_path" is a directory
		fprintf(stderr, "\"%s\" is a directory. Not copied.\n", to_path);
		break;
	    default:	// special file or error
		retval = -1;
		break;
	    }	// switch (filestat(to_path))

	    break;
	case 2:	// directory
	    usb_bulk_read(dh,IFP_BULK_FROM,inbuf,sizeof(inbuf),TOUT);
	    unicode2locale(strbuf, IFP_LSBUFLEN, inbuf, IFP_LSBUFLEN);

	    // "path" "file" -> "path/file"
	    snprintf(ifp_path, IFP_MAXPATHLEN, "%s/%s", fromdir, strbuf);
	    retval = push_dir(ifp_path);

	    break;
	default:
	    break;
	}   // switch (ifp_ctl_ret(ctl))

	if (retval == -1) {
	    goto LS_END;
	}

    }

LS_END: 
    usb_control_msg(dh, IFP_REQ_TYPE, IFP_LS_LEAVE, 0, 0, ctl, sizeof(ctl), TOUT);
    while (pop_dir(strbuf)) {
	// "path" "file" -> "path/file"
	snprintf(to_path, IFP_MAXPATHLEN, "%s/%s", todir, strbuf);

	retval = ifp_download_dir(dh, strbuf, to_path);
	if (retval == -1) {
	    return -1;
	}
    }

    return 0;
}

// NOTE: "name" need normalize.
int ifp_delete_file(usb_dev_handle *dh, const char *name) {
    unsigned char ctl[4];

    ifp_send_filename(dh, name);
    usb_control_msg(dh, IFP_REQ_TYPE, IFP_FILE_DELETE, 0, 0, ctl, sizeof(ctl), TOUT);
    if (!ifp_ok(ctl)) return -1;

    return 0;
}

// based ifp_list_dir_printf()
// NOTE: "name" need normalize.
int ifp_delete_recursive(usb_dev_handle *dh, char *name) {
    int retval = -1;
    unsigned char inbuf[IFP_LSBUFLEN];
    unsigned char strbuf[IFP_LSBUFLEN];
    unsigned char ifp_path[IFP_MAXPATHLEN];
    unsigned char ctl[4];

    if ( (retval = ifp_delete_file(dh, name)) == 0) {
	return retval;
    }

    if (ifp_send_filename(dh, name) < 0)
	return -1;

    usb_control_msg(dh, IFP_REQ_TYPE, IFP_LS, 0, 0, ctl, sizeof(ctl), TOUT);
    if (!ifp_ok(ctl))
	return -1;

    while (1) {
	usb_control_msg(dh, IFP_REQ_TYPE, IFP_LS_NEXT, IFP_FILE | IFP_DIR, 0,
			ctl, sizeof(ctl), TOUT);
	switch (ifp_ctl_ret(ctl)) {
	case 0:	// end
	    goto LS_END;
	    break;

	case 1:	// file
	    usb_bulk_read(dh,IFP_BULK_FROM,inbuf,sizeof(inbuf),TOUT);
	    unicode2locale(strbuf, IFP_LSBUFLEN, inbuf, IFP_LSBUFLEN);

	    // "path" "file" -> "path/file"
	    snprintf(ifp_path, IFP_MAXPATHLEN, "%s/%s", name, strbuf);
	    retval = ifp_delete_file(dh, ifp_path);

	    break;

	case 2:	// directory
	    usb_bulk_read(dh,IFP_BULK_FROM,inbuf,sizeof(inbuf),TOUT);
	    unicode2locale(strbuf, IFP_LSBUFLEN, inbuf, IFP_LSBUFLEN);

	    // "path" "file" -> "path/file"
	    snprintf(ifp_path, IFP_MAXPATHLEN, "%s/%s", name, strbuf);
	    retval = push_dir(ifp_path);

	    break;

	default:
	    break;

	}   // switch (ifp_ctl_ret(ctl))

	if (retval == -1) {
	    goto LS_END;
	}

    }

LS_END: 
    usb_control_msg(dh, IFP_REQ_TYPE, IFP_LS_LEAVE, 0, 0, ctl, sizeof(ctl), TOUT);
    while (pop_dir(strbuf)) {
	if (ifp_delete_recursive(dh, strbuf) == -1) {
	    return -1;
	}
    }

    return ifp_delete_dir(dh, name);
}

// NOTE: "name" need normalize.
int ifp_delete_dir(usb_dev_handle *dh, char *name) {
    unsigned char ctl[4];

    if (strlen(name) <= 0) {
	printf("usage: rmdir directory\n");
	return -1;
    } 

    switch (ifp_empty_dir(dh, name)) {
    case 1:
	fprintf(stderr, "%s: directory not empty\n", name);
	return -1;
	break;

    case -1:	// Fall through
    default:
	fprintf(stderr, "%s: not directory.\n", name);
	return -1;
	break;

    case 0:
	break;

    }

    ifp_send_filename(dh, name);
    usb_control_msg(dh,IFP_REQ_TYPE,IFP_DIR_DELETE,0,0,ctl,sizeof(ctl),TOUT);
    if (!ifp_ok(ctl)) {
	 fprintf(stderr, "rmdir: Could not remove directory. ctl = %s\n",
		 dump_control_regs(ctl, 4));
	 return -1;
    }

    return 0;
}

// NOTE: "name" need normalize.
int ifp_make_dir(usb_dev_handle *dh, char *name) {
    unsigned char ctl[4];

    if (strlen(name) <= 0) {
	fprintf(stderr, "usage: mkdir directory\n");
	return -1;
    } else if (strlen(name) >= IFP_MAXPATHLEN) {
	fprintf(stderr, "ifp mkdir: Pathname too long.\n");
	return -2;
    }

    ifp_send_filename(dh, name);
    usb_control_msg(dh,IFP_REQ_TYPE,IFP_DIR_CREATE,0,0,ctl,sizeof(ctl),TOUT);
    if (!ifp_ok(ctl)) {
	fprintf(stderr, "ERROR: Could not create directory. ctl = %s\n",
	       dump_control_regs(ctl, 4));
	return -1;
    }

    return 0;
}


/****** support functions ************/
int ifp_ctl_ret(unsigned char *control) {
    return control[0] + (control[1]<<8);
}

int ifp_ok(unsigned char *control) {
    return (ifp_ctl_ret(control) == 1);
}

int ifp_send_filename(usb_dev_handle *dh, const char *name) {
    unsigned char buf[IFP_BULK_MAXPATHLEN];
    unsigned char strbuf[IFP_MAXPATHLEN];

    slash2backslash(strbuf, name, strlen(name)+1);
    locale2unicode(buf, IFP_BULK_MAXPATHLEN ,strbuf, IFP_MAXPATHLEN);
    return ifp_send_data(dh,buf,sizeof(buf));
}

int ifp_send_data(usb_dev_handle *dh, char *data, size_t size) {
    unsigned char ctl[8];

    usb_control_msg(dh,IFP_REQ_TYPE,IFP_SEND_DATA,size,0,ctl,sizeof(ctl),TOUT);
    if (!ifp_ok(ctl)) return -1;
    //printf("Send data - %d bytes want, %d allowed\n",size,ctl[4]+ctl[5]*256);
    return usb_bulk_write(dh,IFP_BULK_TO,data,size,TOUT);
}

/**
 * File exists checker(only for file in PC)
 * @param   name    Filename in PC
 * @retval  0	File is not exist.
 * @retval  1	File is a regular file.
 * @retval  2	File is a directory.
 */
int filestat(const char *name) {
    struct stat status;

    if (stat(name, &status) < 0) {
	if (errno == ENOENT) {
	    // file not exist.
	    return 0;
	}
	perror(name);
	return -1;	// error on stat()
    } else {
	if (S_ISREG(status.st_mode)) {
	    return 1;	// Regular file
	} else if (S_ISDIR(status.st_mode)) {
	    return 2;	// Directory
	}
	return 3;	// Special file
    }
}

int ifp_empty_dir(usb_dev_handle *dh, const char *name) {
    int retval;
    unsigned char ctl[4];

    ifp_send_filename(dh,name);

    usb_control_msg(dh,IFP_REQ_TYPE,IFP_LS,0,0,ctl,sizeof(ctl),TOUT);
    if (!ifp_ok(ctl)) return -1;	// not a dir
    usb_control_msg(dh, IFP_REQ_TYPE, IFP_LS_NEXT, IFP_FILE | IFP_DIR, 0,
		    ctl, sizeof(ctl), TOUT);

    retval = (ifp_ctl_ret(ctl) == 0) ? 0 : 1;
    usb_control_msg(dh,IFP_REQ_TYPE,IFP_LS_LEAVE,0,0,ctl,sizeof(ctl),TOUT);
    if (!ifp_ok(ctl)) return -1;	// not a dir

    return retval;
}

int ifp_isdir(usb_dev_handle *dh, const char *name) {
    /* 0: directory 1: not directory */
    return ifp_empty_dir(dh, name) >= 0 ? 1 : 0;
}

int push_dir(char *name) {
    dirlist *d;
    if ( (d = malloc(sizeof(dirlist))) == NULL) {
	return -1;
    }
    //printf("======================== Pushing dir %s\n",name);
    d->name = strdup(name);
    d->next = list;
    list = d;
    return 0;
}

int pop_dir(char *name) {
    dirlist *bak;
    if (list) {
	strcpy(name,list->name);
	//printf("========================= Poping dir %s\n",name);
	bak = list->next;
	free(list->name);
	free(list);
	list = bak;
	return 1;
    } else {
	return 0;
    }
}

int comparefts(const FTSENT **a, const FTSENT **b) {
    if ( (*a)->fts_info == FTS_ERR || (*b)->fts_info == FTS_ERR) {
	return 0;
    }
    if ( (*a)->fts_info == FTS_NS || (*b)->fts_info == FTS_NS) {
	if ( (*a)->fts_info != FTS_NS) {
	    return -1;
	} else if ( (*b)->fts_info != FTS_NS) {
	    return 1;
	} else {
	    return strcmp( (*a)->fts_name, (*b)->fts_name);
	}
    }
    return strcmp( (*a)->fts_name, (*b)->fts_name);
}

// NOTE: "path" need normalize.
char *get_basename(const char *path) {
    char *start;

    for (start = (char*)path; *path != '\0'; path++) {
	if (*path == '/' && *(path+1) != '\0') {
	    start = (char*)++path;
	}
    }
    return start;
}

/*
 * Copy path's dirname to dest
 * and return number of copied charactors
 */
int get_dirname(char *dest, const char *path) {
    int i;
    int lastslash;
    for (i = 0; *path != '\0'; i++) {
	dest[i] = *path++;
	if ('/' == dest[i]) {
	    lastslash = i;
	}
    }
    dest[lastslash] = '\0';
    return lastslash;
}

char *ifp_strcasestr(const char *base, const char *target) {
    size_t len;

    if ( (len = strlen(target)) <= 0) return (char *)base;

    for (; *base != '\0'; base++) {
	if (strncasecmp(base, target, len) == 0) {
	    return (char *)base;
	}
    }

    return NULL;
}

int change_suffix(char *name, const char *newsuffix) {
    size_t i;

    // scan last peliod
    for (i = strlen(name); i >= 0; i--) {
	if (name[i] == '.') {
	    strncpy(name+i, newsuffix, strlen(name) - i);
	    return 0;
	}
    }
    return -1;
}

void normalize_path(char *path, size_t size) {
    char *pos;

    /* /path/to//file/// -> /path/to/file/ */
    remove_double_slash(path, size);

    /* /path/to/file/ -> /path/to/file */
    for (pos = path; *pos != '\0'; pos++)
	;
    if (*(pos-1) == '/') {
	*(pos-1) = '\0';
    }

}

void remove_double_slash(char *path, size_t size) {
    int nocopy = 0;
    int i;
    char *pos;

    for (i = 0, pos = path; i < size-1 && *path != '\0'; i++) {
	if (*path == '/') {
	    if (nocopy == 0) {
		*pos++ = *path++;
		nocopy = 1;
	    } else {
		path++;
	    }
	} else {
	    *pos++ = *path++;
	    nocopy = 0;
	}
    }
    *pos = '\0';
}

void slash2backslash(char *dest, const char *src, size_t count) {
    for (; *src != '\0' && count > 0; count--) {
	if (*src == '/') {
	    *dest++ = '\\'; src++;
	} else {
	    *dest++ = *src++;
	}
    }
    *dest = '\0';
}


int ifp_ctl_datasize(unsigned char *control) {
    return control[4] + (control[5]<<8);
}

char *dump_control_regs(const char *control, int length) {
    int i;
    static char ctl_buffer[1024];
    char hexbuf[3];
    
    memset(ctl_buffer,0,1024);
    strcat(ctl_buffer,"control: ");
    for (i = 0; i < length; i++) {
	sprintf(hexbuf,"%.2x ",control[i]);
	strcat(ctl_buffer,hexbuf);
    }
    
    return ctl_buffer;
}

/**
 * Check file exists in iFP and filetype.
 *
 * usage:
 * // !1 and !2 is same value(zero) in C
 * path = "/path/to/file/mp3";
 * if (!ifp_file_exists(dh, "path") {
 *     fprintf(stderr, "%s is not exist\n", path);
 * }
 *
 * @param[in]   dh	USB device handle
 * @param[in]   name	file path in iFP
 * @retval	0	"name" is not exists or error occured
 * @retval	1	"name" is exists and it's a file.
 * @retval	2	"name" is exists and it's a directory.
 */
int ifp_file_exists(usb_dev_handle *dh, const char *name) {
    int retval = 0;		// "name" isn't exists yet
    int ret = 0;
    unsigned char ctl[4];
    unsigned char lsbuf[IFP_LSBUFLEN];
    unsigned char ascii_lsbuf[IFP_LSBUFLEN];
    unsigned char path[IFP_MAXPATHLEN];
    unsigned char pathbuf[IFP_MAXPATHLEN];

    // path/to -> /path/to
    if (name[0] == '/')
	strcat(pathbuf,name);
    else {
	pathbuf[0] = '/';
	strcat(pathbuf,name);
    }
    // /path/to -> /path
    get_dirname(path, pathbuf);
	    
    if (ifp_send_filename(dh, path) < 0) {
	fprintf(stderr, "ERROR: Could not send dirname: %s.\n", path);
	return 0;
    }
    
    usb_control_msg(dh,IFP_REQ_TYPE,IFP_LS,0,0,ctl,sizeof(ctl),TOUT);
    if (!ifp_ok(ctl)) {
	fprintf(stderr,"ERROR: LS command failed in ifp_file_exists. name = %s, path = %s, ctl = %s.\n", name,path,dump_control_regs(ctl,4));
	return 0;
    }
    
    while (1) {
	usb_control_msg(dh, IFP_REQ_TYPE, IFP_LS_NEXT, IFP_FILE | IFP_DIR, 0,
			ctl, sizeof(ctl), TOUT);
	ret = ifp_ctl_ret(ctl);
	
	switch (ret) {							
	case 0:
	    goto LS_END;

	case 1:				
	    //if (type != 1) break;
	    usb_bulk_read(dh,IFP_BULK_FROM,lsbuf,sizeof(lsbuf),TOUT);
	    unicode2locale(ascii_lsbuf, IFP_LSBUFLEN, lsbuf, IFP_LSBUFLEN);

	    if (strcmp(ascii_lsbuf, get_basename(name)) == 0) {
		retval = 1;	// "name" is exists and it's a file
		goto LS_END;
	    }
	    break;

	case 2:
	    //if (type != 2) break;
	    usb_bulk_read(dh,IFP_BULK_FROM,lsbuf,sizeof(lsbuf),TOUT);
	    //unicode2string(ascii_lsbuf,lsbuf);
	    unicode2locale(ascii_lsbuf, IFP_LSBUFLEN, lsbuf, IFP_LSBUFLEN);
	    if (strcmp(ascii_lsbuf, get_basename(name)) == 0) {
		retval = 2;	// "name" is exists and it's a directory
		goto LS_END;
	    }
	    break;

	default:
	    fprintf(stderr,"Uknown LS_NEXT return value = %d\n", ret);
	    return 0;
	}
    }

LS_END:  
    usb_control_msg(dh,IFP_REQ_TYPE,IFP_LS_LEAVE,0,0,ctl,sizeof(ctl),TOUT);	
    if (!ifp_ok(ctl)) {
	fprintf(stderr,"ERROR: LS LEAVE ****\n");
	return 0;
    }

    return retval;
}

int safe_suffix(usb_dev_handle *dh, const char *name) {
    if ( (ifp_firmware_version(dh) <= 322 ) && isOldIFP &&
	  (ifp_strcasestr(name, ".mp3") ||
	   ifp_strcasestr(name, ".wma") ||
	   ifp_strcasestr(name, ".asf"))
	 ) {
	fprintf(stderr, "%s: this filetype cannot be downloaded.\n", name);
	return 0;   // false
    }

    return 1;	    // true
}

int firmware_check_ifp(const char *firm_name) {
    FILE *fp;
    unsigned char magic[FIRMWARE_HEADER_SIZE] = {0x39, 0xb0, 0x5d, 0xed};
    unsigned char buf[FIRMWARE_HEADER_SIZE];
    char *basename;

    basename = get_basename(firm_name);

    if (strncmp(basename, "IFP-", 4) != 0) {
	fprintf(stderr, "Firmware filename must be set \"IFP-?XXT.HEX\" or \"IFP-1XXTC.HEX\".\n");
	return -1;
    }

    if ( (fp = fopen(firm_name, "r")) == NULL) {
	perror(firm_name);
	return -1;
    }

    if ( fread(buf, sizeof(unsigned char), sizeof(magic), fp)
	    < FIRMWARE_HEADER_SIZE) {
	fprintf(stderr, "ifp firmupdate: invalid firmware file(too short!).\n");
	fclose(fp);
	return -1;
    }
    rewind(fp);	    // DO NOT REMOVE IT!

    if (strncmp(buf, magic, FIRMWARE_HEADER_SIZE) != 0) {
	fprintf(stderr, "ifp firmupdate: Invalid format firmware file.\n");
	fclose(fp);
	return -1;
    }

    fclose(fp);
    return 0;
}

int firmware_check_n10(const char *firm_name) {
    FILE *fp;
    unsigned char magic[8] = {0x37, 0x13, 0x0d, 0xda, 0x32, 0xa2, 0xab, 0x23};
    unsigned char buf[8];
    char *basename;

    basename = get_basename(firm_name);

    if (strncmp(basename, "N10.HEX", 7) != 0) {
	fprintf(stderr, "Firmware filename must be set \"N10.HEX\".\n");
	return -1;
    }

    if ( (fp = fopen(firm_name, "r")) == NULL) {
	perror(firm_name);
	return -1;
    }

    if ( fread(buf, sizeof(unsigned char), sizeof(magic), fp) < 8) {
	fprintf(stderr, "ifp firmupdate: invalid firmware file(too short!).\n");
	fclose(fp);
	return -1;
    }
    rewind(fp);	    // DO NOT REMOVE IT!

    if (strncmp(buf, magic, 8) != 0) {
	fprintf(stderr, "ifp firmupdate: Invalid format firmware file.\n");
	fclose(fp);
	return -1;
    }

    fclose(fp);
    return 0;
}

