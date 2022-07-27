cmd_/home/danilo/code/soa-project/driver/modules.order := {   echo /home/danilo/code/soa-project/driver/test.ko; :; } | awk '!x[$$0]++' - > /home/danilo/code/soa-project/driver/modules.order
