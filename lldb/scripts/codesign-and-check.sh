#!/bin/sh
#
# Usage: codesign-and-check.sh codesign [options ...] BINARY
#
# Runs the code signing command it is given, then checks that the code
# directory hash of the signed binary is unknown to Gatekeeper's ticket
# delivery service. A binary whose CDHash has a ticket is evaluated against
# that ticket when it is launched, so a test inferior that collides with one
# fails in ways that look nothing like a signing problem.
#
# Any outcome other than the service answering that the hash is unknown is an
# error: a lookup that cannot be completed is itself the kind of problem this
# check exists to surface, so it must not be swallowed.

set -u

if [ $# -lt 2 ]; then
    echo "usage: $0 codesign [options ...] BINARY" >&2
    exit 2
fi

codesign=$1
# The binary being signed is the last argument.
for binary; do :; done
# The test Makefiles name their output relative to the build directory, which
# is not enough to locate it from a CI log.
case $binary in
    /*) ;;
    *) binary=$PWD/$binary ;;
esac

error() {
    echo "$0: error: $*" >&2
}

"$@" || exit 1

# A universal binary has one code directory per slice.
cdhashes=$("$codesign" --display --verbose=3 "$binary" 2>&1 | sed -n 's/^CDHash=//p')
if [ -z "$cdhashes" ]; then
    echo "$0: warning: no CDHash for $binary, skipping the Gatekeeper check" >&2
    exit 0
fi

status=0
for cdhash in $cdhashes; do
    # Keep the timeout tight: this runs once per linked test binary.
    if ! response=$(curl --silent --show-error --max-time 10 \
        --write-out 'HTTPSTATUS:%{http_code}' \
        -X POST \
        'https://api.apple-cloudkit.com/database/1/com.apple.gk.ticket-delivery/production/public/records/lookup' \
        -H 'Content-Type: application/json' \
        -H 'X-CloudKit-ContainerId: com.apple.gk.ticket-delivery' \
        -H 'Cache-Control: max-age=300' \
        -d "{\"records\":[{\"recordName\":\"2/2/$cdhash\"}]}"); then
        error "could not reach the ticket delivery service to look up CDHash" \
              "$cdhash of $binary"
        status=1
        continue
    fi
    body=${response%HTTPSTATUS:*}
    http_code=${response##*HTTPSTATUS:}

    if [ -z "$body" ]; then
        error "empty response (HTTP $http_code) from the ticket delivery" \
              "service for CDHash $cdhash of $binary"
        status=1
        continue
    fi

    case $body in
        *NOT_FOUND*)
            # No ticket: the expected outcome for a freshly built binary.
            continue
            ;;
    esac

    if [ "$http_code" != "200" ]; then
        error "ticket delivery service returned HTTP $http_code for CDHash" \
              "$cdhash of $binary"
    else
        error "$binary has a Gatekeeper ticket for CDHash $cdhash and may" \
              "not launch as expected"
    fi
    echo "$body" >&2
    status=1
done

exit $status
