#!/bin/bash
#
# Test decryption capabilities of the Wireshark tools
#
# $Id$
#
# Wireshark - Network traffic analyzer
# By Gerald Combs <gerald@wireshark.org>
# Copyright 2005 Ulf Lamping
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
#


# common exit status values
EXIT_OK=0
EXIT_COMMAND_LINE=1
EXIT_ERROR=2

UAT_FILES="
	80211_keys
	ssl_keys
"

TEST_KEYS_DIR="$PWD/keys/"
if [ "$WS_SYSTEM" == "Windows" ] ; then
    TEST_KEYS_DIR="`cygpath -w $TEST_KEYS_DIR`"
fi

#TS_ARGS="-Tfields -e frame.number -e frame.time_epoch -e frame.time_delta"
TS_DC_ARGS=""
TS_DC_ENV="${HOME_ENV}=${TEST_HOME}"

DIFF_OUT=./diff-output.txt

# We create UATs in the source directory. Add a unique ID so we can avoid
# deleting files we shouldn't.
DC_ID="suite-decryption.sh-$$"


# WPA PSK
decryption_step_80211_wpa_psk() {
	env $TS_DC_ENV $TSHARK $TS_DC_ARGS \
		-o "wlan.enable_decryption: TRUE" \
		-Tfields -e http.request.uri \
		-r captures/wpa-Induction.pcap \
		-R http \
		| grep favicon.ico > /dev/null 2>&1
	RETURNVALUE=$?
	if [ ! $RETURNVALUE -eq $EXIT_OK ]; then
		test_step_failed "Failed to decrypt IEEE 802.11 WPA PSK"
		return
	fi
	test_step_ok
}

# SSL
decryption_step_ssl() {
	env $TS_DC_ENV $TSHARK $TS_DC_ARGS -Tfields -e http.request.uri -r captures/rsasnakeoil2.pcap -R http | grep favicon.ico > /dev/null 2>&1
	RETURNVALUE=$?
	if [ ! $RETURNVALUE -eq $EXIT_OK ]; then
		test_step_failed "Failed to decrypt SSL"
		return
	fi
	test_step_ok
}

tshark_decryption_suite() {
	test_step_add "IEEE 802.11 WPA PSK Decryption" decryption_step_80211_wpa_psk
	test_step_add "SSL Decryption" decryption_step_ssl
}

decryption_cleanup_step() {
	for UAT in $UAT_FILES ; do
		grep $DC_ID $WS_BIN_PATH/$UAT > /dev/null 2>&1
		RETURNVALUE=$?
		if [ $RETURNVALUE -eq $EXIT_OK ]; then
			rm -f $WS_BIN_PATH/$UAT
		fi
	done
	rm -rf fakehome
}

decryption_prep_step() {
	decryption_cleanup_step
	mkdir fakehome

	for UAT in $UAT_FILES ; do
		if [ -f $WS_BIN_PATH/$UAT ] ; then
			test_remark_add "$WS_BIN_PATH/$UAT exists. One or more tests may fail."
		else
			echo "# Created by $DC_ID" > $WS_BIN_PATH/$UAT
			sed -e "s|TEST_KEYS_DIR|${TEST_KEYS_DIR//\\/\\\\x5c}|" < ./config/$UAT.tmpl >> $WS_BIN_PATH/$UAT
		fi
	done
}

decryption_suite() {
	test_step_set_pre decryption_prep_step
	test_step_set_post decryption_cleanup_step
	test_suite_add "TShark decryption" tshark_decryption_suite
}

# Editor modelines
#
# Local Variables:
# sh-basic-offset: 8
# tab-width: 8
# indent-tabs-mode: t
# End:
#
# ex: set shiftwidth=8 tabstop=8 noexpandtab:
# :indentSize=8:tabSize=8:noTabs=false:
