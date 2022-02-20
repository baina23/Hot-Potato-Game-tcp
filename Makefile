TARGETS=ringmaster player

all: $(TARGETS)
clean:
	rm -f $(TARGETS)

ringmaster: ringmaster.cpp potato.h
	g++ -g -o $@ $<

player: player.cpp potato.h
	g++ -g -o $@ $<