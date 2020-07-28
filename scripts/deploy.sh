#!/bin/bash

set -euxo

ssh-keyscan -H etf2.org >> ~/.ssh/known_hosts
ssh etf@etf2.org mkdir -p /tmp/etfdeploy
scp $TRAVIS_BUILD_DIR/*.7z etf@etf2.org:/tmp/etfdeploy
ssh etf@etf2.org 7z x /tmp/etfdeploy/*.7z -aoa -o/home/etf/wolfet
ssh etf@etf2.org rm -rf /tmp/etfdeploy
ssh etf@etf2.org sudo systemctl restart etf.service

echo "Deployment successful"
