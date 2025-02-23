#!/usr/bin/env bash

exec   pip install flask
exec ./launch_chffrplus.sh
exec echo -en "1" > /data/params/d/AleSato_SecondBoot
