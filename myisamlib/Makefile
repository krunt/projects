BASEDIR=$(PWD)
BUILDDIR=$(BASEDIR)/build
MYSQLDIR=$(BUILDDIR)/mysql-5.1.65
MYSQLTARGZ=$(BASEDIR)/mysql-5.1.65.tar.gz
MYSQLINSTALLDIR=/usr/share/myisamlib

CC=g++
CCFLAGS= -g -O3 -fPIC
LDFLAGS=-L$(BUILDDIR)/lib -lmysqldlib -lmyisamreader

O_FILES=$(BUILDDIR)/table_iterator.o \
		$(BUILDDIR)/myisamlib_init.o $(BUILDDIR)/table.o

$(BUILDDIR)/%.o: $(BASEDIR)/src/%.cc
	$(CC) $(CCFLAGS) -I$(BASEDIR)/include -c $^ -o $@

myisamlib: $(O_FILES) mysql
	@mkdir -p $(BUILDDIR)/lib
	$(CC) $(CCFLAGS) -shared -o $(BUILDDIR)/lib/libmyisamlib.so $(O_FILES) 
	@touch myisamlib

myisamtests: myisamlib
	$(CC) $(CCFLAGS) -o $(BUILDDIR)/test $(BASEDIR)/tests/test.cc \
		-I$(BASEDIR)/include  -lmyisamlib $(LDFLAGS) -lboost_filesystem
	$(CC) $(CCFLAGS) -o $(BUILDDIR)/filtered_test $(BASEDIR)/tests/filtered_test.cc \
		-I$(BASEDIR)/include  -lmyisamlib $(LDFLAGS) -lboost_filesystem

mysql:
	@mkdir -p $(BUILDDIR)/lib
	/bin/bash $(BASEDIR)/build_mysql.sh \
		$(BASEDIR) $(BUILDDIR) \
		$(MYSQLDIR) $(MYSQLTARGZ) 
	@touch mysql

mysql_install: mysql
	/bin/bash $(BASEDIR)/build_mysql.sh \
		$(BASEDIR)    \
		$(BUILDDIR)   \
		$(MYSQLDIR)   \
		$(MYSQLTARGZ) \
		1 # install-only flag

install: myisamlib
	/usr/bin/install -m 0755 $(BUILDDIR)/lib/libmyisamlib.so /usr/lib
	/usr/bin/install -m 0755 $(BUILDDIR)/lib/libmyisamreader.so /usr/lib
	/usr/bin/install -m 0755 $(BUILDDIR)/lib/libmyisamreader.so.0 /usr/lib
	/usr/bin/install -m 0755 $(BUILDDIR)/lib/libmyisamreader.so.0.0.0 /usr/lib
	/usr/bin/install -m 0755 $(BUILDDIR)/lib/libmysqldlib.so /usr/lib
	/usr/bin/install -m 0755 $(BUILDDIR)/lib/libmysqldlib.so.0 /usr/lib
	/usr/bin/install -m 0755 $(BUILDDIR)/lib/libmysqldlib.so.0.0.0 /usr/lib
	/usr/bin/install -m 0644 $(BASEDIR)/include/MyIsamLib.h /usr/include
	/usr/bin/install -m 0644 $(BASEDIR)/include/Table.h /usr/include
	/usr/bin/install -m 0644 $(BASEDIR)/include/TableIterator.h /usr/include
	/usr/bin/install -m 0644 $(BASEDIR)/include/myisamreader.h /usr/include
	/usr/bin/install -m 0644 $(BASEDIR)/include/mysqldlib.h /usr/include
	mkdir -p $(MYSQLINSTALLDIR)/support_files $(MYSQLINSTALLDIR)/support_files/datadir
	cp -r $(BASEDIR)/support_files/datadir/mysql $(MYSQLINSTALLDIR)/support_files/datadir
	cp -r $(BASEDIR)/support_files/share $(MYSQLINSTALLDIR)/support_files
	cp -r $(BASEDIR)/support_files/plugin $(MYSQLINSTALLDIR)/support_files

uninstall:
	rm -rf /usr/lib/libmyisamlib.so /usr/lib/libmyisamreader{.so,.so.0,.so.0.0.0} \
		/usr/lib/libmysqldlib{.so,.so.0,.so.0.0.0} $(MYSQLINSTALLDIR)

clean:
	rm -f $(BUILDDIR)/*.o $(BUILDDIR)/test $(BUILDDIR)/filtered_test 
	rm -rf $(MYSQLDIR) $(BUILDDIR)/lib mysql myisamlib
