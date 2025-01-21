#!/bin/sh

#
# The following environment variables must be set prior to invoking
# this script:
#
# UACME_DOMAIN       specifies the domain for which the certificate
#                    shoult be issed, e.g. genodians.org
# UACME_PRODUCTION   specifies the intent to use the production server
#                    rather than the staging one used for testing
# WEBDAV_USER        specifies the username for WebDAV access
# WEBDAV_PASSWORD    specifies the password for WebDAV access
#

if [ -z "$UACME_DOMAIN" ]; then
	echo "$(basename "$0") requires UACME_DOMAIN environment variable set"
	exit 1
fi

# CONFDIR
#   private/key.pem               account private key
#   private/UACME_DOMAIN/key.pem  UACME_DOMAIN private key     - privkey.pem
#   UACME_DOMAIN/cert.pem         certificate for UACME_DOMAIN - fullchain.pem
FORCE="--force"
CONFDIR="--confdir uacme.d"
CRYPTO="--type EC --bits 384"
VERBOSE="-v -v -v"

[ -z "$UACME_PRODUCTION" ] && STAGING="--staging" || STAGING=""

check_webdav_credentials() {
	local prog_name="$1"
	if [ -z "$WEBDAV_USER" ] || [ -z "$WEBDAV_PASSWORD" ]; then
		echo "$prog_name requires WEBDAV_USER and WEBDAV_PASSWORD environment variables set"
		exit 1
	fi
}

if [ "$1" = "uacme-issue" ]; then
	check_webdav_credentials "uacme-issue"

	uacme ${FORCE} ${STAGING} ${VERBOSE} ${CONFDIR} ${CRYPTO} \
	      --hook $0 issue ${UACME_DOMAIN} www.${UACME_DOMAIN}
	exit $?
fi

if [ "$1" = "user-conf" ]; then
	check_webdav_credentials "user-conf"

	echo $WEBDAV_USER:upload:$(echo -n $WEBDAV_USER:upload:$WEBDAV_PASSWORD | sha256sum | head -c 64)
	exit $?
fi

#
# uacme challenge hook script follows
#
# The hook not always executed accord. to the Let's encrypt FAQ.
#
# Once you successfully complete the challenges for a domain, the resulting
# authorization is cached for your account to use again later. Cached
# authorizations last for 30 days from the time of validation. If the
# certificate you requested has all of the necessary authorizations cached then
# validation will not happen again until the relevant cached authorizations
# expire.
#

if [ $# -ne 5 ]; then
	echo "Usage:"
	echo "  $(basename "$0") method type ident token auth   hook-script mode"
	echo "  $(basename "$0") uacme-issue                    issue new certificate"
	echo "  $(basename "$0") user-conf                      generate upload-user.conf contents"
	exit 85
fi

METHOD=$1
TYPE=$2
IDENT=$3
TOKEN=$4
AUTH=$5

if [ "$TYPE" != http-01 ]; then
	exit 1
fi

#echo "--- METHOD=${METHOD} TYPE=${TYPE} IDENT=${IDENT} TOKEN=${TOKEN} AUTH=${AUTH}"

WEBDAV="curl --verbose --insecure --digest --user $WEBDAV_USER:$WEBDAV_PASSWORD"

case "$METHOD" in
	"begin")
		printf "%s" "${AUTH}" > TOKEN
		$WEBDAV --upload-file TOKEN https://${UACME_DOMAIN}/upload/acme-challenge/${TOKEN}
		exit $?
		;;

	"done")
		rm TOKEN
		$WEBDAV --request DELETE https://${UACME_DOMAIN}/upload/acme-challenge/${TOKEN}
		$WEBDAV --upload-file uacme.d/${UACME_DOMAIN}/cert.pem https://${UACME_DOMAIN}/upload/cert/fullchain.pem
		exit $?
		;;

	"failed")
		rm TOKEN
		$WEBDAV --request DELETE https://${UACME_DOMAIN}/upload/acme-challenge/${TOKEN}
		exit $?
		;;

	*)
		echo "$0: invalid method" 1>&2
		exit 1
esac

#
# Notes
#
# openssl s_client -connect ${UACME_DOMAIN}:443 -servername ${UACME_DOMAIN} | openssl x509 -noout -dates
# openssl rsa  -in privkey.pem   -text -noout
# openssl x509 -in fullchain.pem -text -noout
# openssl req  -new -x509 -keyout privkey.pem -out fullchain.pem -days 365 -nodes
#
# uacme [--staging] --verbose --type EC --bits 384 --confdir $CONFDIR new [EMAIL]
# uacme [--staging] --verbose --type EC --bits 384 --confdir $CONFDIR update [EMAIL]
# uacme [--staging] --verbose --type EC --bits 384 --confdir $CONFDIR --hook ./uacme.sh issue ${UACME_DOMAIN}
# uacme [--staging] --verbose --type EC --bits 384 --confdir $CONFDIR revoke ${UACME_DOMAIN}/cert.pem
#
# curl --insecure --digest --user user:pw --upload-file TOKEN         https://${UACME_DOMAIN}/upload/acme-challenge/
# curl --insecure --digest --user user:pw --upload-file fullchain.pem https://${UACME_DOMAIN}/upload/cert/
# curl --insecure --digest --user user:pw --request DELETE            https://${UACME_DOMAIN}/upload/acme-challenge/TOKEN
#
# echo user:upload:$(echo -n user:upload:pw | sha256sum | head -c 64) > upload-user.conf
