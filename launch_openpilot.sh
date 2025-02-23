#!/usr/bin/env bash

exec pip install flask
exec echo -en "1" > /data/params/d/AleSato_SecondBoot
exec ./launch_chffrplus.sh
