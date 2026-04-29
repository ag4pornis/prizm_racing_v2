SOURCES = $(wildcard src/*)

all: sdl prizm gint

sdl: sdl/racing

prizm: prizm/racing_v2.g3a

gint: gint/racing_v2.g3a

clean:
	make $(MFLAGS) -C sdl/ clean
	make $(MFLAGS) -C prizm/ clean
	make $(MFLAGS) -C gint/ clean

sdl/racing: $(SOURCES)
	make $(MFLAGS) -C sdl/

prizm/racing_v2.g3a: $(SOURCES)
	make $(MFLAGS) -C prizm/

gint/racing_v2.g3a: $(SOURCES)
	make $(MFLAGS) -C gint/

package: release/racing_v2.zip release/racing_v2.tar.gz

release/racing_v2.zip: prizm/racing_v2.g3a
	mkdir -p release
	zip -j release/racing_v2.zip LICENSE prizm/racing_v2.g3a

release/racing_v2.tar.gz: prizm/racing_v2.g3a
	mkdir -p release
	tar czvf release/racing_v2.tar.gz LICENSE -C prizm racing_v2.g3a

package-clean:
	rm release -rf
