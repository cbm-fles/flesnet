#!/bin/bash

influx -username admin -password admin -execute "show databases" 2> /dev/null | grep -q cbm01_data
