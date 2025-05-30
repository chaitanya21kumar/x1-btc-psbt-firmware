#!/bin/bash
# Copyright 2019 Chaitanya Kumar
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.


DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null && pwd )"
PROJECT_NAME="$(basename $(realpath "$DIR/.."))"
CONTAINER_NAME=$PROJECT_NAME-dev

if [ -n "$CONTAINER_RUNTIME" ]; then
  RUNTIME="$CONTAINER_RUNTIME"
elif command -v podman &>/dev/null; then
  RUNTIME=podman
else
  RUNTIME=docker
fi

function docker_cleanup {
    $RUNTIME exec $IMAGE bash -c "if [ -f $PIDFILE ]; then kill -TERM -\$(cat $PIDFILE); rm $PIDFILE; fi"
}

# See https://github.com/moby/moby/issues/9098#issuecomment-189743947.
function docker_exec {
    IMAGE=$1
    PIDFILE=/tmp/docker-exec-$$
    shift
    trap 'kill $PID; docker_cleanup $IMAGE $PIDFILE' TERM INT

    USERFLAG=""
    if [ "$RUNTIME" = "docker" ] ; then
        # Only needed for docker - see the comments in dockerenv.sh.
        USERFLAG="--user=dockeruser"
    fi

    $RUNTIME exec $USERFLAG --workdir="$DIR/.." -i $IMAGE bash -c "echo \"\$\$\" > $PIDFILE; eval $*" &
    PID=$!
    wait $PID
    RESULT=$?
    if [ ! $RESULT -ne 0 ]
    then
        exit $RESULT
    fi
    trap - TERM INT
    wait $PID
    RESULT=$?
    if [ $RESULT -ne 0 ]
    then
        exit $RESULT
    fi
}

docker_exec $CONTAINER_NAME "$@"
