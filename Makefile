
pipesim: ./src/pipesim.c
	cc ./src/pipesim.c -Wall -Werror -o ./bin/pipesim

testrun: pipesim
	./bin/pipesim

test: testrun
