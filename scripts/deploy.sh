#!/bin/bash

set -euxo

ssh etf@et2.org mkdir /tmp/etfdeploy
scp $TRAVIS_BUILD_DIR/*.7z etf@etf2.org:/tmp/etfdeploy
ssh etf@etf2.org 7z x /tmp/etfdeploy/*.7z
ssh etf@etf2.org cp /tmp/etfdeploy/et-ded.x86 ~/wolfet
ssh etf@etf2.org rm -rf /tmp/etfdeploy
ssh etf@etf2.org sudo systemctl restart etf.service

echo success
