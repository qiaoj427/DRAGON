#!/bin/sh

PREFIX=/usr/local/dragon

ZEBRA_DAEMON=$PREFIX/sbin/zebra
ZEBRA_ARGS="-d -f /usr/local/dragon/etc/zebra.conf"

OSPF_DAEMON=$PREFIX/sbin/ospfd
OSPF_ARGS="-d -f /usr/local/dragon/etc/ospfd.conf"

RSVP_DAEMON=$PREFIX/bin/RSVPD
RSVP_ARGS="-c /usr/local/dragon/etc/RSVPD.conf -d -o /var/log/RSVPD.log -L select"

DRAGON_DAEMON=$PREFIX/bin/dragon
DRAGON_ARGS="-d -f /usr/local/dragon/etc/dragon.conf"

# for NARBs we need 2 ospfd instances.  the above
# OSPF_DAEMON/OSPF_ARGS variables are not used for NARBs:

OSPF_INTER_DAEMON=$PREFIX/sbin/ospfd
OSPF_INTER_ARGS="-d -I -P 2604 -f /usr/local/dragon/etc/ospfd-inter.conf"

OSPF_INTRA_DAEMON=$PREFIX/sbin/ospfd
OSPF_INTRA_ARGS="-d -P 2614 -f /usr/local/dragon/etc/ospfd-intra.conf"

NARB_DAEMON=$PREFIX/sbin/narb
NARB_ARGS="-d"

#
# need to have a way for 'stop' to recognize/kill both ospfd instances...
#

#
# See which daemons are already running...
#

case "`uname`" in
        Linux* | *BSD* | Darwin*)
                zebra_pid=`ps      ax | awk '{if (match($5, ".*/zebra$")       || $5 == "zebra")  print $1}'`
                rsvp_pid=`ps       ax | awk '{if (match($5, ".*/RSVPD$")       || $5 == "RSVPD")  print $1}'`
                dragon_pid=`ps     ax | awk '{if (match($5, ".*/dragon$")      || $5 == "dragon") print $1}'`
                narb_pid=`ps       ax | awk '{if (match($5, ".*/narb$")        || $5 == "narb")   print $1}'`

                # XXX ugh...there must be a better way to do this...this is a kludge
                # maybe search for OSPF_INTER_ARGS and OSPF_INTRA_ARGS...or OSPF_ARGS?
                ospf_intra_pid=`ps ax | awk '{if (match($0, ".*/ospfd-intra")  || match($0, ".*/ospfd\.conf")) print $1}'`
                ospf_inter_pid=`ps ax | awk '{if (match($0, ".*/ospfd-inter")) print $1}'`
                ;;
        *)
                zebra_pid=""
                ospf_intra_pid=""
                ospf_inter_pid=""
                rsvp_pid=""
                dragon_pid=""
                narb_pid=""
                ;;
esac

#
# Start or stop the daemons based upon the first argument to the script.
#

case $1 in
    start-vlsr | startvlsr | restart-vlsr)
        if test "$zebra_pid" != ""; then
	    kill $zebra_pid
	fi
	$ZEBRA_DAEMON $ZEBRA_ARGS
	if test $? != 0; then
	    echo "dragon-sw: unable to start zebra daemon."
	    exit 1
	fi
	echo "dragon-sw: started zebra daemon."
    
        # XXX again, a bit of a hack here...
        if test "$ospf_intra_pid" != ""; then
	    kill $ospf_intra_pid
	fi
	$OSPF_DAEMON $OSPF_ARGS
	if test $? != 0; then
	    echo "dragon-sw: unable to start ospf daemon."
	    exit 1
	fi
	echo "dragon-sw: started ospf daemon."
    
        if test "$rsvp_pid" != ""; then
	    kill $rsvp_pid
	fi
	$RSVP_DAEMON $RSVP_ARGS
	if test $? != 0; then
	    echo "dragon-sw: unable to start rsvp daemon."
	    exit 1
	fi
	echo "dragon-sw: started rsvp daemon."
    
        if test "$dragon_pid" != ""; then
	    kill $dragon_pid
	fi
	$DRAGON_DAEMON $DRAGON_ARGS
	if test $? != 0; then
	    echo "dragon-sw: unable to start dragon daemon."
	    exit 1
	fi
	echo "dragon-sw: started dragon daemon."
	;;
    
    start-narb | startnarb | restart-narb)
        if test "$zebra_pid" != ""; then
	    kill $zebra_pid
	fi
	$ZEBRA_DAEMON $ZEBRA_ARGS
	if test $? != 0; then
	    echo "dragon-sw: unable to start zebra daemon."
	    exit 1
	fi
	echo "dragon-sw: started zebra daemon."

        if test "$ospf_inter_pid" != ""; then
	    kill $ospf_inter_pid
	fi
	$OSPF_INTER_DAEMON $OSPF_INTER_ARGS
	if test $? != 0; then
	    echo "dragon-sw: unable to start ospf inter-domain daemon."
	    exit 1
	fi
	echo "dragon-sw: started ospf inter-domain daemon."

        if test "$ospf_intra_pid" != ""; then
	    kill $ospf_intra_pid
	fi
	$OSPF_INTRA_DAEMON $OSPF_INTRA_ARGS
	if test $? != 0; then
	    echo "dragon-sw: unable to start ospf intra-domain daemon."
	    exit 1
	fi
	echo "dragon-sw: started ospf intra-domain daemon."

        echo "sleeping for 10 seconds before starting narb daemon...please stand by."
        sleep 10
    
        if test "$narb_pid" != ""; then
	    kill $narb_pid
	fi
	$NARB_DAEMON $NARB_ARGS
	if test $? != 0; then
	    echo "dragon-sw: unable to start narb daemon."
	    exit 1
	fi
	echo "dragon-sw: started narb daemon."
	;;
    
    stop)   
        if test "$zebra_pid" != ""; then
	    kill $zebra_pid
	    echo "dragon-sw: stopped zebra daemon."
	fi

        if test "$rsvp_pid" != ""; then
	    kill $rsvp_pid
	    echo "dragon-sw: stopped rsvp daemon."
	fi

        if test "$ospf_intra_pid" != ""; then
	    kill $ospf_intra_pid
	    echo "dragon-sw: stopped intra-domain ospf daemon."
	fi

        if test "$ospf_inter_pid" != ""; then
	    kill $ospf_inter_pid
	    echo "dragon-sw: stopped inter-domain ospf daemon."
	fi

        if test "$dragon_pid" != ""; then
	    kill $dragon_pid
	    echo "dragon-sw: stopped dragon daemon."
	fi

        if test "$narb_pid" != ""; then
	    kill $narb_pid
	    echo "dragon-sw: stopped narb daemon."
	fi
	;;
    
    status) 
        if test "$zebra_pid" != ""; then
	    echo "dragon-sw: zebra daemon is running."
	else
	    echo "dragon-sw: zebra daemon is NOT running."
	fi

        if test "$rsvp_pid" != ""; then
	    echo "dragon-sw: rsvp daemon is running."
	else
	    echo "dragon-sw: rsvp daemon is NOT running."
	fi

        if test "$ospf_inter_pid" != ""; then
	    echo "dragon-sw: inter-domain ospf daemon is running."
	fi

        if test "$ospf_intra_pid" != ""; then
	    echo "dragon-sw: intra-domain ospf daemon is running."
	fi

        if test "$dragon_pid" != ""; then
	    echo "dragon-sw: dragon daemon is running."
	else
	    echo "dragon-sw: dragon daemon is NOT running."
	fi

        if test "$narb_pid" != ""; then
	    echo "dragon-sw: narb daemon is running."
	else
	    echo "dragon-sw: narb daemon is NOT running."
	fi
	;;
    
    *)
        echo "Usage: $0 {start-vlsr|restart-vlsr|start-narb|restart-narb|status|stop}"
	exit 1
	;;
esac

#
# Exit with no errors.
#

exit 0
