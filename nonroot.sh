#!/bin/sh
# $Id: nonroot.sh,v 1.8 2005/11/05 01:46:51 yamajun Exp $
#
# nonroot.sh -- Add permission to usb device file for run ifp by non-root user

# uid check
if [ ! `id -u` = 0 ]; then
    echo "You are not root.  Cannot run this script."
    exit
fi

case `uname` in
Linux)
    if [ ! -d /etc/hotplug ]; then
	echo "This script need hotplugging support."
	exit
    fi

    # cite from giriver README.

    # if group "ifp" exist, groupadd exit with error message.
    /usr/sbin/groupadd ifp
    echo
    echo "****************************************************************"
    echo "Please add iFP users to group \"ifp\"."
    echo "****************************************************************"
    echo

    if egrep -q '^ifpdev[[:blank:]]' /etc/hotplug/usb.usermap; then
	echo "/etc/hotplug/usb.usermap already changed."
	echo "Please modify yourself for your system."

    else
	cat >> /etc/hotplug/usb.usermap << EOF

# for iRiver iFP MP3 player(iFP 1xx series - N10)
ifpdev 0x0003 0x4102 0x1001 0x0000 0x0000 0x00 0x00 0x00 0x00 0x00 0x00 0x00000000
ifpdev 0x0003 0x4102 0x1003 0x0000 0x0000 0x00 0x00 0x00 0x00 0x00 0x00 0x00000000
ifpdev 0x0003 0x4102 0x1005 0x0000 0x0000 0x00 0x00 0x00 0x00 0x00 0x00 0x00000000
ifpdev 0x0003 0x4102 0x1007 0x0000 0x0000 0x00 0x00 0x00 0x00 0x00 0x00 0x00000000
ifpdev 0x0003 0x4102 0x1008 0x0000 0x0000 0x00 0x00 0x00 0x00 0x00 0x00 0x00000000
ifpdev 0x0003 0x4102 0x1009 0x0000 0x0000 0x00 0x00 0x00 0x00 0x00 0x00 0x00000000
ifpdev 0x0003 0x4102 0x1010 0x0000 0x0000 0x00 0x00 0x00 0x00 0x00 0x00 0x00000000
ifpdev 0x0003 0x4102 0x1011 0x0000 0x0000 0x00 0x00 0x00 0x00 0x00 0x00 0x00000000

EOF
	echo "/etc/hotplug/usb.usermap changed."
    fi

    if [ ! -d /etc/hotplug/usb ]; then
	mkdir /etc/hotplug/usb
    fi
    if [ ! -x /etc/hotplug/usb/ifpdev ]; then
	echo "/etc/hotplug/usb/ifpdev added"; \
	cat > /etc/hotplug/usb/ifpdev << EOF
#!/bin/sh
# /etc/hotplug/usb/ifpdev

chgrp ifp \$DEVICE
chmod g+rw \$DEVICE
EOF
	chmod 755 /etc/hotplug/usb/ifpdev
    fi
    ;;

FreeBSD)
    if [ -f /etc/defaults/devfs.rules ]; then
	# for after FreeBSD 5.2-RELEASE. 
	if egrep -q '^add[[:blank:]]path[[:blank:]][[:punct:]]*ugen' /etc/devfs.rules; then
	    echo "****************************************************************"
	    echo "/etc/devfs.rules already changed."
	    echo "Please modify yourself for your system."

	else 
	    echo "/etc/devfs.rules changed."; \
	    cat >> /etc/devfs.rules << EOF

# for iRiver iFP MP3 player
[iriver_ruleset=4012]
add	path	'ugen*'	mode	664

EOF

	    if egrep -q '^devfs_system_ruleset=' /etc/rc.conf; then
		echo "****************************************************************"
		echo "/etc/rc.conf already changed."
		echo "Please modify yourself for your system."
		echo
		echo "example /etc/devfs.rules:"
		echo "	[iriver_ruleset=4012]"
		echo "	add path 'ugen*' mode 664"
		echo 
		echo "example /etc/rc.conf:"
		echo "	devfs_system_ruleset=\"iriver_ruleset\""

	    else
		echo 'devfs_system_ruleset="iriver_ruleset"' >> /etc/rc.conf; \
		echo "/etc/rc.conf changed."; \
		/etc/rc.d/devfs start
	    fi
	fi
		
    else
	# for 4.*
	chmod g+rw /dev/usb* /dev/ugen*
    fi

    echo
    echo "****************************************************************"
    echo "Please add iFP users to group \"operator\"."
    echo "****************************************************************"
    echo
    ;;

NetBSD)
    chmod g+rw /dev/usb* /dev/ugen*

    echo
    echo "****************************************************************"
    echo "Please add iFP users to group \"wheel\"."
    echo "****************************************************************"
    echo
    ;;

Darwin)
    # Mac OS X
    echo "No need to configuration for run ifp by non-root user on Mac OS X"
    ;;

*)
    echo "Sorry, this script did not support your OS."
    ;;
esac

