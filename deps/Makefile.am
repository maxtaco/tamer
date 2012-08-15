
TAG = Debug

LIBUV = uv/$(TAG)/obj.target/libuv.a
uv_srcdir=$(srcdir)/uv

$(LIBUV):
	echo $(LIBUV)
	mkdir -p uv
	if [ ! -L $(uv_srcdir)/out ] ; then ln -s `pwd`/uv $(uv_srcdir)/out ; fi 
	cd $(uv_srcdir) && ./gyp_uv -f make 
	SD=$(uv_srcdir) perl -ne ' { s#srcdir := ..#srcdir = $$ENV{"SD"}#; print }'  -i uv/Makefile
	cd uv && $(MAKE)


all: $(LIBUV)
default: $(LIBUV)

clean:
	rm -rf uv

.PHONY: clean
