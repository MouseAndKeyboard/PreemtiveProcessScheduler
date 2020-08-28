

pipesim: ./src/pipesim.c
	cc ./src/pipesim.c -Wall -Werror -o ./bin/pipesim

test_exit: pipesim
	./bin/pipesim ./test/test_exit 1000 2000 > ./test/output
	diff ./test/r_test_exit ./test/output

test: test_exit
