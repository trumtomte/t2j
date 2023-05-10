##
# [t]orrent [2]to [j]SON
#
# @file
# @version 0.1

objects = main.o gc.o print.o

t2j: $(objects)
	$(CC) -o t2j main.o gc.o print.o -Wall -Wextra -pedantic -std=c11

main.o: node.h
gc.o: gc.h
print.o: print.h

# Which is the same as:
#
# 	main.o: main.c node.h
# 		$(CC) -c main.c
#
# 	etc.

.PHONY: clean

clean:
	rm t2j $(objects)

# end
