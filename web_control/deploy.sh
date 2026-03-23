#!/bin/bash
UNO_Q="arduino@larine.local"
DEPLOY_DIR="/home/arduino/mower-web"
ssh $UNO_Q "mkdir -p $DEPLOY_DIR"
scp web_control/server.py web_control/requirements.txt web_control/index.html web_control/mower-web.service $UNO_Q:$DEPLOY_DIR/
ssh $UNO_Q "pip3 install -r $DEPLOY_DIR/requirements.txt --quiet"
ssh $UNO_Q "mkdir -p ~/.config/systemd/user && cp $DEPLOY_DIR/mower-web.service ~/.config/systemd/user/ && systemctl --user daemon-reload && systemctl --user enable mower-web.service && systemctl --user restart mower-web.service"
echo "Deployed to UNO Q!"
