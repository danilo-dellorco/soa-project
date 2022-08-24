cmd_/home/danilo/code/soa-project/driver/test.mod := printf '%s\n'   test.o | awk '!x[$$0]++ { print("/home/danilo/code/soa-project/driver/"$$0) }' > /home/danilo/code/soa-project/driver/test.mod
