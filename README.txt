Place these indexed BMP sprites in the assets directory. The build should
generate the matching bn_sprite_items_*.h headers from them at build time
via grit:

sips1.bmp                (card body)
sips2.bmp                (upper accent)
sips3.bmp                (lower accent)

The three sprites are composed into the single SIPS card in card.cpp.

Keep all three sprites on a shared palette so their indexed colors match.

Build
-----

The Makefile uses the Butano checkout at ../butano-master/butano and scans
assets for graphics. From this directory, run:

make

This requires devkitARM, Butano's tools, Python, and make to be available in
the build environment. A successful build creates sips.gba in the build
directory.
