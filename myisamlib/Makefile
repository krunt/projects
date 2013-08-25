BASEDIR=$(PWD)
BUILDDIR=$(BASEDIR)/build
MYSQLDIR=$(BUILDDIR)/mysql-5.1.65
MYSQLTARGZ=$(BASEDIR)/../stuff/experiments/mysql-5.1.65.tar.gz
MYSQLINSTALLDIR=/usr/share/myisamlib

CC=g++
CCFLAGS= -g -O3 -fPIC
LDFLAGS=-L$(BUILDDIR)/lib -lmysqldlib -lmyisamreader

myisamlib: 
	$(CC) $(CCFLAGS) -I$(BASEDIR)/include -c $(BASEDIR)/src/table_iterator.cc \
		-o $(BUILDDIR)/table_iterator.o
	$(CC) $(CCFLAGS) -I$(BASEDIR)/include -c $(BASEDIR)/src/table.cc \
		-o $(BUILDDIR)/table.o
	$(CC) $(CCFLAGS) -I$(BASEDIR)/include -c $(BASEDIR)/src/myisamlib_init.cc  \
		-o $(BUILDDIR)/myisamlib_init.o
	$(CC) $(CCFLAGS) -shared -o $(BUILDDIR)/lib/libmyisamlib.so \
		$(BUILDDIR)/table_iterator.o $(BUILDDIR)/table.o $(BUILDDIR)/myisamlib_init.o $(LDFLAGS)

myisamtests: myisamlib
	$(CC) $(CCFLAGS) -o $(BUILDDIR)/test $(BASEDIR)/tests/test.cc \
		-I$(BASEDIR)/include $(LDFLAGS) -lmyisamlib -lboost_filesystem
	$(CC) $(CCFLAGS) -o $(BUILDDIR)/filtered_test $(BASEDIR)/tests/filtered_test.cc \
		-I$(BASEDIR)/include $(LDFLAGS) -lmyisamlib -lboost_filesystem

mysql:
	/bin/bash $(BASEDIR)/build_mysql.sh $(BASEDIR) $(BUILDDIR) $(MYSQLDIR) $(MYSQLTARGZ) 

install: 
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
	rm -rf $(MYSQLDIR) $(BUILDDIR)/lib
